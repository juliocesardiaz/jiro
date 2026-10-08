import AppKit
import CoreAudio
import AudioToolbox

struct AudioOutput: Identifiable, Equatable {
    var id: String { uid }
    let objectID: AudioObjectID
    let uid: String
    let name: String
    let sampleRate: Double
}

struct PlayingApp: Identifiable {
    var id: String { bundleID }
    let bundleID: String
    let name: String
    let icon: NSImage?
    let processes: [AudioObjectID]
    let isPlaying: Bool
}

enum AudioHardware {
    static let system = AudioObjectID(kAudioObjectSystemObject)

    static func address(_ selector: AudioObjectPropertySelector,
                        _ scope: AudioObjectPropertyScope = kAudioObjectPropertyScopeGlobal
    ) -> AudioObjectPropertyAddress {
        AudioObjectPropertyAddress(mSelector: selector, mScope: scope,
                                   mElement: kAudioObjectPropertyElementMain)
    }

    static func value<T>(_ object: AudioObjectID, _ selector: AudioObjectPropertySelector,
                         _ initial: T,
                         scope: AudioObjectPropertyScope = kAudioObjectPropertyScopeGlobal) -> T? {
        var property = address(selector, scope)
        var result = initial
        var size = UInt32(MemoryLayout<T>.size)
        let status = withUnsafeMutablePointer(to: &result) {
            AudioObjectGetPropertyData(object, &property, 0, nil, &size, $0)
        }
        return status == noErr ? result : nil
    }

    static func text(_ object: AudioObjectID, _ selector: AudioObjectPropertySelector) -> String? {
        var property = address(selector)
        var result: CFString = "" as CFString
        var size = UInt32(MemoryLayout<CFString>.size)
        let status = withUnsafeMutablePointer(to: &result) {
            AudioObjectGetPropertyData(object, &property, 0, nil, &size, $0)
        }
        return status == noErr ? result as String : nil
    }

    static func objects(_ object: AudioObjectID, _ selector: AudioObjectPropertySelector,
                        scope: AudioObjectPropertyScope = kAudioObjectPropertyScopeGlobal
    ) -> [AudioObjectID] {
        var property = address(selector, scope)
        var size: UInt32 = 0
        guard AudioObjectGetPropertyDataSize(object, &property, 0, nil, &size) == noErr,
              size > 0 else { return [] }
        var result = [AudioObjectID](repeating: 0, count: Int(size) / MemoryLayout<AudioObjectID>.stride)
        let status = result.withUnsafeMutableBytes {
            AudioObjectGetPropertyData(object, &property, 0, nil, &size, $0.baseAddress!)
        }
        return status == noErr ? Array(result.prefix(Int(size) / MemoryLayout<AudioObjectID>.stride)) : []
    }

    static func channelCount(_ device: AudioObjectID, scope: AudioObjectPropertyScope) -> Int {
        var property = address(kAudioDevicePropertyStreamConfiguration, scope)
        var size: UInt32 = 0
        guard AudioObjectGetPropertyDataSize(device, &property, 0, nil, &size) == noErr,
              size >= MemoryLayout<AudioBufferList>.size else { return 0 }
        let memory = UnsafeMutableRawPointer.allocate(byteCount: Int(size),
                                                     alignment: MemoryLayout<AudioBufferList>.alignment)
        defer { memory.deallocate() }
        guard AudioObjectGetPropertyData(device, &property, 0, nil, &size, memory) == noErr else { return 0 }
        let list = memory.assumingMemoryBound(to: AudioBufferList.self)
        return UnsafeMutableAudioBufferListPointer(list).reduce(0) { $0 + Int($1.mNumberChannels) }
    }

    static var defaultOutputUID: String? {
        guard let id = value(system, kAudioHardwarePropertyDefaultOutputDevice, AudioObjectID(0))
        else { return nil }
        return text(id, kAudioDevicePropertyDeviceUID)
    }

    static func outputs() -> [AudioOutput] {
        objects(system, kAudioHardwarePropertyDevices).compactMap { id in
            guard channelCount(id, scope: kAudioDevicePropertyScopeOutput) >= 2,
                  let uid = text(id, kAudioDevicePropertyDeviceUID),
                  !uid.hasPrefix("SonicPatch.demo."),
                  let name = text(id, kAudioObjectPropertyName),
                  let rate = value(id, kAudioDevicePropertyNominalSampleRate, Double(0)), rate > 0
            else { return nil }
            return AudioOutput(objectID: id, uid: uid, name: name, sampleRate: rate)
        }.sorted { $0.name.localizedStandardCompare($1.name) == .orderedAscending }
    }

    // Include idle members in a playing app's group. Keep existing taps alive
    // over playback pauses: muting must never remove its own capture source.
    static func apps() -> [PlayingApp] {
        var groups: [String: [AudioObjectID]] = [:]
        for id in objects(system, kAudioHardwarePropertyProcessObjectList) {
            guard let bundle = text(id, kAudioProcessPropertyBundleID), !bundle.isEmpty,
                  bundle != Bundle.main.bundleIdentifier,
                  value(id, kAudioProcessPropertyPID, pid_t(-1)) != getpid()
            else { continue }
            groups[bundle, default: []].append(id)
        }
        return groups.map { bundle, ids in
            let pid = value(ids[0], kAudioProcessPropertyPID, pid_t(-1)) ?? -1
            let running = NSRunningApplication(processIdentifier: pid)
            return PlayingApp(bundleID: bundle, name: running?.localizedName ?? bundle,
                              icon: running?.icon, processes: ids.sorted(),
                              isPlaying: ids.contains {
                                  value($0, kAudioProcessPropertyIsRunningOutput, UInt32(0)) == 1
                              })
        }.sorted { $0.name.localizedStandardCompare($1.name) == .orderedAscending }
    }
}

struct AudioFailure: LocalizedError {
    let message: String
    var errorDescription: String? { message }
    static func check(_ status: OSStatus, _ action: String) throws {
        guard status == noErr else {
            throw AudioFailure(message: "\(action) failed (Core Audio \(status)). Check audio-capture permission, then Retry.")
        }
    }
}

// Owns one private aggregate and exactly one C++ render context. No shared graph
// can be rendered concurrently by two apps. All lifecycle calls are on main.
final class DemoTap {
    let processes: [AudioObjectID]
    let output: AudioOutput
    private(set) var startedAt = Date()
    private var tap: AudioObjectID = 0
    private var aggregate: AudioObjectID = 0
    private var ioProc: AudioDeviceIOProcID?
    private var strip: OpaquePointer?

    init(app: PlayingApp, output: AudioOutput, settings: AppMix) throws {
        self.processes = app.processes
        self.output = output
        do {
            let description = CATapDescription(stereoMixdownOfProcesses: app.processes)
            description.name = "SonicPatch · \(app.name)"
            description.isPrivate = true
            description.muteBehavior = .mutedWhenTapped
            try AudioFailure.check(AudioHardwareCreateProcessTap(description, &tap), "Capture")
            guard let format = AudioHardware.value(tap, kAudioTapPropertyFormat, AudioStreamBasicDescription()),
                  Self.isFloatPCM(format), format.mChannelsPerFrame == 2,
                  let tapUID = AudioHardware.text(tap, kAudioTapPropertyUID)
            else { throw AudioFailure(message: "This app did not provide a stereo Float32 audio stream.") }

            let spec: [String: Any] = [
                kAudioAggregateDeviceNameKey: "SonicPatch · \(app.name)",
                kAudioAggregateDeviceUIDKey: "SonicPatch.demo.\(UUID().uuidString)",
                kAudioAggregateDeviceIsPrivateKey: true,
                kAudioAggregateDeviceIsStackedKey: false,
                kAudioAggregateDeviceMainSubDeviceKey: output.uid,
                kAudioAggregateDeviceTapAutoStartKey: true,
                kAudioAggregateDeviceSubDeviceListKey: [[kAudioSubDeviceUIDKey: output.uid]],
                kAudioAggregateDeviceTapListKey: [[kAudioSubTapUIDKey: tapUID,
                                                  kAudioSubTapDriftCompensationKey: true]]
            ]
            try AudioFailure.check(AudioHardwareCreateAggregateDevice(spec as CFDictionary, &aggregate), "Output setup")

            // Physical input channels precede sub-tap channels in the aggregate.
            // Never copy a duplex interface's microphone into the output.
            let physicalInputs = AudioHardware.channelCount(output.objectID, scope: kAudioDevicePropertyScopeInput)
            let aggregateInputs = AudioHardware.channelCount(aggregate, scope: kAudioDevicePropertyScopeInput)
            guard aggregateInputs == physicalInputs + 2 else {
                throw AudioFailure(message: "Unsupported input layout. Try the Mac's built-in speakers or wired headphones.")
            }
            let inputStreams = AudioHardware.objects(aggregate, kAudioDevicePropertyStreams,
                                                     scope: kAudioDevicePropertyScopeInput)
            let outputStreams = AudioHardware.objects(aggregate, kAudioDevicePropertyStreams,
                                                      scope: kAudioDevicePropertyScopeOutput)
            guard !inputStreams.isEmpty, !outputStreams.isEmpty else {
                throw AudioFailure(message: "The output device has no usable audio streams.")
            }
            // Validate actual callback formats, not only the source tap format.
            let tapStreams = inputStreams.filter {
                (AudioHardware.value($0, kAudioStreamPropertyStartingChannel, UInt32(0)) ?? 0) > UInt32(physicalInputs)
            }
            guard !tapStreams.isEmpty else { throw AudioFailure(message: "Could not locate the captured app's audio channels.") }
            for stream in tapStreams + outputStreams {
                guard let pcm = AudioHardware.value(stream, kAudioStreamPropertyVirtualFormat, AudioStreamBasicDescription()),
                      Self.isFloatPCM(pcm), abs(pcm.mSampleRate - output.sampleRate) < 1 else {
                    throw AudioFailure(message: "This output's current format is unsupported. Try built-in speakers or a stereo 44.1/48 kHz output.")
                }
            }
            guard let state = SPStripCreate(output.sampleRate, UInt32(physicalInputs)) else {
                throw AudioFailure(message: "Could not prepare the audio processor.")
            }
            strip = state
            update(settings)
            try AudioFailure.check(SPStripInstallIOProc(aggregate, state, &ioProc), "Audio callback")
            try AudioFailure.check(AudioDeviceStart(aggregate, ioProc), "Playback")
            startedAt = Date()
        } catch {
            stop()
            throw error
        }
    }

    private static func isFloatPCM(_ f: AudioStreamBasicDescription) -> Bool {
        let channels = (f.mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0 ? 1 : f.mChannelsPerFrame
        return f.mFormatID == kAudioFormatLinearPCM && f.mBitsPerChannel == 32
            && (f.mFormatFlags & kAudioFormatFlagIsFloat) != 0
            && (f.mFormatFlags & kAudioFormatFlagIsBigEndian) == 0
            && f.mBytesPerFrame == channels * 4
    }

    func update(_ settings: AppMix) {
        SPStripSetControls(strip, Float(settings.volume), Float(settings.balance), settings.muted)
    }
    func takePeak() -> Float { SPStripTakePeak(strip) }
    var callbackCount: UInt32 { SPStripCallbackCount(strip) }
    var formatFault: Bool { SPStripHasFormatFault(strip) }

    func stop() {
        if let proc = ioProc {
            AudioDeviceStop(aggregate, proc)
            AudioDeviceDestroyIOProcID(aggregate, proc)
            ioProc = nil
        }
        if aggregate != 0 { AudioHardwareDestroyAggregateDevice(aggregate); aggregate = 0 }
        if tap != 0 { AudioHardwareDestroyProcessTap(tap); tap = 0 }
        if let state = strip { SPStripDestroy(state); strip = nil }
    }
    deinit { stop() }
}
