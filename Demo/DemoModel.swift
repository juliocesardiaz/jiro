import AppKit
import Combine

struct AppMix: Codable {
    var volume: Double = 1
    var balance: Double = 0
    var muted = false
    var outputUID = "" // empty means follow the system default

    var validated: AppMix {
        var copy = self
        copy.volume = volume.isFinite ? min(1, max(0, volume)) : 1
        copy.balance = balance.isFinite ? min(1, max(-1, balance)) : 0
        return copy
    }
}

// All access runs on the main run loop. The C++ contexts alone cross to audio
// threads, using atomic controls and metering. No Swift code runs in an IOProc.
final class DemoModel: ObservableObject {
    @Published private(set) var enabled = false
    @Published private(set) var apps: [PlayingApp] = []
    @Published private(set) var outputs: [AudioOutput] = []
    @Published private(set) var defaultOutputName = "No output"
    @Published private(set) var mixes: [String: AppMix] = [:]
    @Published private(set) var levels: [String: Float] = [:]
    @Published private(set) var errors: [String: String] = [:]
    @Published private(set) var active: Set<String> = []
    private var taps: [String: DemoTap] = [:]
    private var scanTimer: Timer?
    private var meterTimer: Timer?
    private var observers: [NSObjectProtocol] = []
    private let settingsKey = "SonicPatch.demo.mixes.v1"
    private var lastCallbacks: [String: UInt32] = [:]
    private var lastActivity: [String: Date] = [:]

    init() {
        if let data = UserDefaults.standard.data(forKey: settingsKey),
           let saved = try? JSONDecoder().decode([String: AppMix].self, from: data) {
            mixes = saved.mapValues { $0.validated }
        }
        // Start is explicit on every launch so opening the demo never grabs
        // another application's audio before the user sees the consent screen.
        refresh()
        scanTimer = timer(every: 1) { [weak self] in self?.refresh() }
        meterTimer = timer(every: 1.0 / 30.0) { [weak self] in self?.pollMeters() }
        observers.append(NotificationCenter.default.addObserver(
            forName: NSApplication.willTerminateNotification, object: nil, queue: .main
        ) { [weak self] _ in self?.stop() })
        observers.append(NSWorkspace.shared.notificationCenter.addObserver(
            forName: NSWorkspace.willSleepNotification, object: nil, queue: .main
        ) { [weak self] _ in self?.stop() })
    }

    private func timer(every interval: TimeInterval, action: @escaping () -> Void) -> Timer {
        let timer = Timer(timeInterval: interval, repeats: true) { _ in action() }
        RunLoop.main.add(timer, forMode: .common)
        return timer
    }

    func mix(_ id: String) -> AppMix { mixes[id] ?? AppMix() }

    func change(_ id: String, _ mutation: (inout AppMix) -> Void) {
        var value = mix(id)
        let previousOutput = value.outputUID
        mutation(&value)
        value = value.validated
        mixes[id] = value
        if let data = try? JSONEncoder().encode(mixes) {
            UserDefaults.standard.set(data, forKey: settingsKey)
        }
        taps[id]?.update(value)
        if previousOutput != value.outputUID { retry(id) }
    }

    func start() {
        errors.removeAll()
        enabled = true
        refresh()
    }

    func stop() {
        enabled = false
        for tap in taps.values { tap.stop() }
        taps.removeAll()
        active.removeAll()
        levels.removeAll()
        errors.removeAll()
        lastCallbacks.removeAll()
        lastActivity.removeAll()
    }

    func retry(_ id: String) {
        removeTap(id)
        errors[id] = nil
        refresh()
    }

    private func removeTap(_ id: String) {
        taps.removeValue(forKey: id)?.stop()
        active.remove(id)
        levels[id] = nil
        lastCallbacks[id] = nil
        lastActivity[id] = nil
    }

    func refresh() {
        let discovered = AudioHardware.apps()
        let previouslySeen = Set(apps.map(\.id))
        apps = discovered.filter { $0.isPlaying || previouslySeen.contains($0.id) }
        outputs = AudioHardware.outputs()
        let defaultUID = AudioHardware.defaultOutputUID
        defaultOutputName = outputs.first { $0.uid == defaultUID }?.name ?? "No stereo output"
        let existingIDs = Set(apps.map(\.id))
        for id in Array(taps.keys) where !existingIDs.contains(id) { removeTap(id) }
        guard enabled else { return }

        for app in apps {
            let settings = mix(app.id)
            let targetUID = settings.outputUID.isEmpty ? defaultUID : settings.outputUID
            guard let output = outputs.first(where: { $0.uid == targetUID }) else {
                removeTap(app.id)
                errors[app.id] = "Output unavailable. Choose a connected stereo output and Retry."
                continue
            }
            // Refresh captures on app-helper replacement, output switching,
            // default-output changes, or sample-rate/device changes.
            if let current = taps[app.id],
               current.processes != app.processes || current.output != output {
                removeTap(app.id)
                errors[app.id] = nil
            }
            if let current = taps[app.id] {
                let count = current.callbackCount
                if count != lastCallbacks[app.id] {
                    lastCallbacks[app.id] = count
                    lastActivity[app.id] = Date()
                }
                let last = lastActivity[app.id] ?? current.startedAt
                if current.formatFault || (app.isPlaying && Date().timeIntervalSince(last) > 5) {
                    removeTap(app.id)
                    errors[app.id] = "Audio stopped responding. Capture was released. Check system audio recording permission, then Retry."
                }
                continue
            }
            guard errors[app.id] == nil, app.isPlaying else { continue }
            do {
                let tap = try DemoTap(app: app, output: output, settings: settings)
                taps[app.id] = tap
                active.insert(app.id)
                lastActivity[app.id] = Date()
            } catch {
                errors[app.id] = error.localizedDescription
            }
        }
    }

    private func pollMeters() {
        var next: [String: Float] = [:]
        for (id, tap) in taps {
            next[id] = max(tap.takePeak(), (levels[id] ?? 0) * 0.82)
        }
        levels = next
    }

    func openAudioPrivacy() {
        if let url = URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_ScreenCapture") {
            NSWorkspace.shared.open(url)
        }
    }
}
