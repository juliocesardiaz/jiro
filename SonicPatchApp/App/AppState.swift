//
//  AppState.swift
//  SonicPatch
//
//  Top-level observable application state. Owns the engine bridge, the list of
//  audio sources, selection, run state, and a metering poll loop.
//

import Foundation
import Combine

/// Observable state shared across the SwiftUI scene graph.
///
/// Threading: all `@Published` mutations happen on the main thread. The metering
/// loop currently uses a `Timer`; see the TODO about moving to a
/// `CADisplayLink`-style driver reading engine atomics.
@MainActor
final class AppState: ObservableObject {

    // MARK: Published state

    /// Audio-producing applications currently known to the app.
    @Published private(set) var sources: [AudioSource] = []

    /// The source whose channel strip is shown in the mixer window.
    @Published var selectedSourceID: AudioSource.ID?

    /// Whether the C++ engine is running.
    @Published private(set) var isRunning: Bool = false

    /// Current engine sample rate (display only).
    @Published private(set) var sampleRate: Double = 48_000

    /// Most recent per-strip level readings, keyed by strip id, for the meters.
    @Published private(set) var levels: [EngineBridge.StripID: LevelReading] = [:]

    // MARK: Collaborators

    private let engine = EngineBridge()
    private let appMonitor = AppMonitor()
    private let tapManager: TapManager

    private var cancellables = Set<AnyCancellable>()
    private var meterTimer: Timer?

    /// Convenience accessor for the selected source.
    var selectedSource: AudioSource? {
        guard let id = selectedSourceID else { return sources.first }
        return sources.first { $0.id == id }
    }

    // MARK: Lifecycle

    init() {
        self.tapManager = TapManager(engine: engine)
        bindAppMonitor()
        start()
    }

    /// Starts the engine, begins monitoring audible apps, and starts metering.
    func start() {
        engine.start()
        isRunning = engine.isRunning()
        appMonitor.start()
        startMeterLoop()
    }

    /// Tears everything down (called on quit).
    func stop() {
        stopMeterLoop()
        appMonitor.stop()
        tapManager.removeAllTaps()
        engine.stop()
        isRunning = engine.isRunning()
    }

    // MARK: App monitoring → sources

    private func bindAppMonitor() {
        appMonitor.$audibleApps
            .receive(on: RunLoop.main)
            .sink { [weak self] apps in
                self?.reconcileSources(with: apps)
            }
            .store(in: &cancellables)
    }

    /// Reconcile the published source list with the set of audible apps,
    /// creating/removing taps and engine strips as apps come and go.
    private func reconcileSources(with apps: [AudioApp]) {
        var updated: [AudioSource] = []
        var seenBundleIds = Set<String>()
        for app in apps {
            // Dedupe by bundle id: two entries with the same id would produce
            // duplicate SwiftUI ForEach identities (undefined behavior).
            guard !seenBundleIds.contains(app.bundleId) else { continue }
            seenBundleIds.insert(app.bundleId)

            if var existing = sources.first(where: { $0.bundleId == app.bundleId }) {
                existing.displayName = app.displayName
                updated.append(existing)
            } else {
                // New audible app: spin up a tap and an engine strip. A failed
                // strip (invalidStrip) gets no tap and no stripID, so controls
                // and meters for it are visibly inert instead of silently
                // targeting strip 0.
                let strip = engine.createChannelStrip(bundleId: app.bundleId)
                var source = AudioSource(bundleId: app.bundleId,
                                         displayName: app.displayName)
                if strip != EngineBridge.invalidStrip {
                    source.stripID = strip
                    tapManager.startTap(forBundleId: app.bundleId, stripID: strip)
                }
                source.icon = app.icon
                updated.append(source)
            }
        }
        // Remove sources for apps that are no longer audible.
        for gone in sources where !seenBundleIds.contains(gone.bundleId) {
            tapManager.stopTap(forBundleId: gone.bundleId)
            if let strip = gone.stripID { engine.removeChannelStrip(strip) }
        }
        sources = updated

        // The audible set changed — this is exactly the signal deferred taps
        // wait on (a tap fails until its process actually produces audio).
        tapManager.retryPendingTaps()

        // Heal the selection if it's nil OR points at a removed source.
        if selectedSourceID == nil
            || !sources.contains(where: { $0.id == selectedSourceID }) {
            selectedSourceID = sources.first?.id
        }
    }

    // MARK: Control intents (called from views)

    func setVolume(_ db: Float, for source: AudioSource) {
        guard let strip = source.stripID else { return }
        engine.setVolume(strip, db: db)
        mutate(source.id) { $0.volumeDB = db }
    }

    func setMute(_ muted: Bool, for source: AudioSource) {
        guard let strip = source.stripID else { return }
        engine.setMute(strip, muted: muted)
        mutate(source.id) { $0.muted = muted }
    }

    func setPan(_ pan: Float, for source: AudioSource) {
        guard let strip = source.stripID else { return }
        engine.setPan(strip, pan: pan)
        mutate(source.id) { $0.pan = pan }
    }

    private func mutate(_ id: AudioSource.ID, _ body: (inout AudioSource) -> Void) {
        guard let idx = sources.firstIndex(where: { $0.id == id }) else { return }
        body(&sources[idx])
    }

    // MARK: Metering loop

    /// A simple ~60 fps poll loop for the level meters.
    ///
    /// TODO(Phase 2): replace this `Timer` with a `CADisplayLink`-driven loop and
    /// read the engine's lock-free `LevelSnapshot` atomics directly, instead of
    /// hopping through Combine. The current Timer is fine for the PoC.
    private func startMeterLoop() {
        meterTimer?.invalidate()
        let timer = Timer(timeInterval: 1.0 / 60.0, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.pollLevels() }
        }
        RunLoop.main.add(timer, forMode: .common)
        meterTimer = timer
    }

    private func stopMeterLoop() {
        meterTimer?.invalidate()
        meterTimer = nil
    }

    private func pollLevels() {
        var snapshot: [EngineBridge.StripID: LevelReading] = [:]
        for source in sources {
            guard let strip = source.stripID else { continue }
            // Phase 1 routes audio Tap -> output directly, so the live peak comes
            // from the tap's IOProc. Phase 2 moves processing into the engine, at
            // which point `engine.getLevel` carries the authoritative meter.
            let engineLevel = engine.getLevel(strip)
            let tapPeak = tapManager.takePeak(forBundleId: source.bundleId)
            snapshot[strip] = LevelReading(peak: max(engineLevel.peak, tapPeak),
                                           rms: engineLevel.rms)
        }
        levels = snapshot
    }
}

/// A UI-facing copy of the engine's `LevelSnapshot` (linear peak/rms).
struct LevelReading: Equatable {
    var peak: Float = 0
    var rms: Float = 0

    /// Peak expressed in dBFS, clamped for display.
    var peakDB: Float { Self.linearToDB(peak) }

    static func linearToDB(_ linear: Float) -> Float {
        guard linear > 0.000_01 else { return -120 }
        return 20 * log10f(linear)
    }
}
