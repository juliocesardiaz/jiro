//
//  ProcessTap.swift
//  SonicPatch
//
//  Phase 1 centerpiece: a *real* Core Audio process tap that captures one
//  application's audio and passes it through to the default output device, while
//  computing a peak level for the meters.
//
//  Lifecycle:
//    1. Resolve the target process' AudioObjectID from its bundle id.
//    2. Build a CATapDescription (stereo mixdown of that process).
//    3. AudioHardwareCreateProcessTap -> tap object id.
//    4. Read the tap's stream format (kAudioTapPropertyFormat) and UID
//       (kAudioTapPropertyUID).
//    5. Create a *private* aggregate device that contains both the tap (as a
//       sub-tap) and the current default output device (as the main sub-device),
//       so a single IOProc runs on one clock.
//    6. Install an IOProc via AudioDeviceCreateIOProcIDWithBlock that copies the
//       tapped input straight to the output buffers (pass-through) and records
//       the block peak. Start IO.
//    7. On teardown, stop/destroy the IOProc, the aggregate device, and the tap.
//
//  Notes:
//    * The tapped process is muted at the source (`.mutedWhenTapped`) so audio is
//      heard only via SonicPatch's pass-through, not doubled.
//    * In Phase 2 the IOProc will deposit captured frames into the C++ engine's
//      TapSourceNode and fetch the processed mix from a DeviceSinkNode instead of
//      copying input straight to output. The plumbing (single private aggregate +
//      one IOProc) stays the same.
//

import Foundation
#if canImport(AppKit)
import AppKit
#endif

#if canImport(CoreAudio)
import CoreAudio
import AudioToolbox

/// Errors that can occur while creating or running a tap.
enum ProcessTapError: Error {
    case processNotFound
    case outputDeviceUnavailable
    case tapCreationFailed(OSStatus)
    case aggregateDeviceCreationFailed(OSStatus)
    case ioProcCreationFailed(OSStatus)
    case formatUnavailable
}

/// Owns one process tap, its private aggregate device, and the pass-through
/// IOProc that drives audio for a specific strip.
final class ProcessTap {

    let bundleId: String
    let stripID: EngineBridge.StripID

    /// Negotiated tap format, read from kAudioTapPropertyFormat.
    private(set) var sampleRate: Double = 0
    private(set) var channelCount: UInt32 = 0
    private(set) var isRunning = false

    // Core Audio resource ids.
    private var tapObjectID       = AudioObjectID(kAudioObjectUnknown)
    private var aggregateDeviceID = AudioObjectID(kAudioObjectUnknown)
    private var ioProcID: AudioDeviceIOProcID?

    /// State shared with the IOProc. Heap-allocated so the real-time block
    /// captures a plain pointer with no ARC traffic on the audio thread.
    private struct IOState {
        var peakLinear: Float = 0
    }
    private let state = UnsafeMutablePointer<IOState>.allocate(capacity: 1)

    init(bundleId: String, stripID: EngineBridge.StripID) {
        self.bundleId = bundleId
        self.stripID = stripID
        state.initialize(to: IOState())
    }

    deinit {
        tearDown()
        state.deinitialize(count: 1)
        state.deallocate()
    }

    // MARK: Activation

    /// Build the tap + aggregate device and start IO. Throws on any CA failure.
    /// Throws `.processNotFound` when the app isn't audible yet; `TapManager`
    /// catches that and retries when the process becomes audible.
    func activate() throws {
        let processObject = try resolveProcessObject(forBundleId: bundleId)
        let outputUID = try defaultOutputDeviceUID()
        try createTap(forProcess: processObject)
        try readTapFormat()
        let tapUID = try readTapUID()
        try createPrivateAggregateDevice(outputUID: outputUID, tapUID: tapUID)
        try installIOProc()
        try startIO()
        isRunning = true
    }

    /// Stop IO and release all Core Audio resources. Idempotent.
    func tearDown() {
        guard tapObjectID != AudioObjectID(kAudioObjectUnknown)
                || aggregateDeviceID != AudioObjectID(kAudioObjectUnknown) else { return }
        stopIO()
        destroyIOProc()
        destroyAggregateDevice()
        destroyTap()
        isRunning = false
    }

    /// Read and reset the most recent peak (linear 0...1). Called from the UI's
    /// metering loop on the main thread.
    func takePeak() -> Float {
        let peak = state.pointee.peakLinear
        state.pointee.peakLinear = 0
        return peak
    }

    // MARK: Steps

    /// Resolve the Core Audio process object for `bundleId`.
    private func resolveProcessObject(forBundleId bundleId: String) throws -> AudioObjectID {
        #if canImport(AppKit)
        guard let app = NSRunningApplication
            .runningApplications(withBundleIdentifier: bundleId).first else {
            throw ProcessTapError.processNotFound
        }
        var inputPID = app.processIdentifier
        #else
        throw ProcessTapError.processNotFound
        #endif

        var address = AudioObjectPropertyAddress(
            mSelector: kAudioHardwarePropertyTranslatePIDToProcessObject,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMain)

        var processObject = AudioObjectID(kAudioObjectUnknown)
        var size = UInt32(MemoryLayout<AudioObjectID>.size)

        let status = withUnsafeMutablePointer(to: &inputPID) { pidPtr -> OSStatus in
            AudioObjectGetPropertyData(
                AudioObjectID(kAudioObjectSystemObject),
                &address,
                UInt32(MemoryLayout<pid_t>.size),
                pidPtr,
                &size,
                &processObject)
        }
        // A process with no current audio object isn't tappable yet — defer.
        guard status == noErr,
              processObject != AudioObjectID(kAudioObjectUnknown) else {
            throw ProcessTapError.processNotFound
        }
        return processObject
    }

    /// UID of the current default output device, used as the aggregate's main
    /// sub-device so pass-through audio reaches the user's speakers/headphones.
    private func defaultOutputDeviceUID() throws -> CFString {
        var address = AudioObjectPropertyAddress(
            mSelector: kAudioHardwarePropertyDefaultOutputDevice,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMain)
        var deviceID = AudioObjectID(kAudioObjectUnknown)
        var size = UInt32(MemoryLayout<AudioObjectID>.size)
        var status = AudioObjectGetPropertyData(
            AudioObjectID(kAudioObjectSystemObject), &address, 0, nil, &size, &deviceID)
        guard status == noErr, deviceID != AudioObjectID(kAudioObjectUnknown) else {
            throw ProcessTapError.outputDeviceUnavailable
        }

        var uidAddress = AudioObjectPropertyAddress(
            mSelector: kAudioDevicePropertyDeviceUID,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMain)
        var uid = "" as CFString
        var uidSize = UInt32(MemoryLayout<CFString>.size)
        status = withUnsafeMutablePointer(to: &uid) {
            AudioObjectGetPropertyData(deviceID, &uidAddress, 0, nil, &uidSize, $0)
        }
        guard status == noErr else { throw ProcessTapError.outputDeviceUnavailable }
        return uid
    }

    /// Create the process tap from a CATapDescription.
    private func createTap(forProcess processObject: AudioObjectID) throws {
        let description = CATapDescription(stereoMixdownOfProcesses: [processObject])
        description.name = "SonicPatch Tap (\(bundleId))"
        description.isPrivate = true
        // Mute the app at the source so we don't double the audio with our
        // pass-through copy.
        description.muteBehavior = .mutedWhenTapped

        var tap = AudioObjectID(kAudioObjectUnknown)
        let status = AudioHardwareCreateProcessTap(description, &tap)
        guard status == noErr, tap != AudioObjectID(kAudioObjectUnknown) else {
            throw ProcessTapError.tapCreationFailed(status)
        }
        tapObjectID = tap
    }

    /// Read the tap's stream format (rate / channel count).
    private func readTapFormat() throws {
        var address = AudioObjectPropertyAddress(
            mSelector: kAudioTapPropertyFormat,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMain)
        var asbd = AudioStreamBasicDescription()
        var size = UInt32(MemoryLayout<AudioStreamBasicDescription>.size)
        let status = AudioObjectGetPropertyData(tapObjectID, &address, 0, nil, &size, &asbd)
        guard status == noErr else { throw ProcessTapError.formatUnavailable }
        sampleRate   = asbd.mSampleRate
        channelCount = asbd.mChannelsPerFrame
    }

    /// Read the tap's UID so it can be referenced from the aggregate's tap list.
    private func readTapUID() throws -> CFString {
        var address = AudioObjectPropertyAddress(
            mSelector: kAudioTapPropertyUID,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMain)
        var uid = "" as CFString
        var size = UInt32(MemoryLayout<CFString>.size)
        let status = withUnsafeMutablePointer(to: &uid) {
            AudioObjectGetPropertyData(tapObjectID, &address, 0, nil, &size, $0)
        }
        guard status == noErr else { throw ProcessTapError.formatUnavailable }
        return uid
    }

    /// Create a *private* aggregate device combining the tap and the output
    /// device, so one IOProc captures and renders on a single clock.
    private func createPrivateAggregateDevice(outputUID: CFString, tapUID: CFString) throws {
        let aggregateUID = "SonicPatch.aggregate.\(bundleId).\(UUID().uuidString)"
        let description: [String: Any] = [
            kAudioAggregateDeviceNameKey:          "SonicPatch (\(bundleId))",
            kAudioAggregateDeviceUIDKey:           aggregateUID,
            kAudioAggregateDeviceMainSubDeviceKey: outputUID,
            kAudioAggregateDeviceIsPrivateKey:     true,   // not user-visible
            kAudioAggregateDeviceIsStackedKey:     false,
            kAudioAggregateDeviceTapAutoStartKey:  true,
            kAudioAggregateDeviceSubDeviceListKey: [
                [ kAudioSubDeviceUIDKey: outputUID ],
            ],
            kAudioAggregateDeviceTapListKey: [
                [ kAudioSubTapUIDKey: tapUID ],
            ],
        ]
        var device = AudioObjectID(kAudioObjectUnknown)
        let status = AudioHardwareCreateAggregateDevice(description as CFDictionary, &device)
        guard status == noErr, device != AudioObjectID(kAudioObjectUnknown) else {
            throw ProcessTapError.aggregateDeviceCreationFailed(status)
        }
        aggregateDeviceID = device
    }

    /// Install the pass-through IOProc on the aggregate device.
    private func installIOProc() throws {
        let statePtr = state   // captured as a plain pointer (no ARC on the RT thread)

        var proc: AudioDeviceIOProcID?
        let status = AudioDeviceCreateIOProcIDWithBlock(
            &proc,
            aggregateDeviceID,
            nil
        ) { (_, inInputData, _, outOutputData, _) in
            // [RT] No allocation, no locks, no ARC. Copy tapped input to output
            // and record the block peak.
            let input  = UnsafeMutableAudioBufferListPointer(
                UnsafeMutablePointer(mutating: inInputData))
            let output = UnsafeMutableAudioBufferListPointer(outOutputData)

            var blockPeak: Float = 0
            let pairs = min(input.count, output.count)

            var i = 0
            while i < pairs {
                let inBuf  = input[i]
                let outBuf = output[i]
                let bytes  = min(inBuf.mDataByteSize, outBuf.mDataByteSize)
                if let src = inBuf.mData, let dst = outBuf.mData {
                    memcpy(dst, src, Int(bytes))
                    let count = Int(bytes) / MemoryLayout<Float>.size
                    let samples = src.assumingMemoryBound(to: Float.self)
                    var s = 0
                    while s < count {
                        let v = abs(samples[s])
                        if v > blockPeak { blockPeak = v }
                        s += 1
                    }
                }
                i += 1
            }

            // Silence any output buffers with no matching input.
            var j = pairs
            while j < output.count {
                if let dst = output[j].mData {
                    memset(dst, 0, Int(output[j].mDataByteSize))
                }
                j += 1
            }

            // Publish the running peak. A naturally-aligned 32-bit float write is
            // effectively atomic on arm64; this is a benign meter race that the
            // engine's real lock-free LevelSnapshot atomics replace in Phase 2.
            if blockPeak > statePtr.pointee.peakLinear {
                statePtr.pointee.peakLinear = blockPeak
            }
        }

        guard status == noErr, let proc else {
            throw ProcessTapError.ioProcCreationFailed(status)
        }
        ioProcID = proc
    }

    private func startIO() throws {
        let status = AudioDeviceStart(aggregateDeviceID, ioProcID)
        guard status == noErr else { throw ProcessTapError.ioProcCreationFailed(status) }
    }

    // MARK: Teardown steps

    private func stopIO() {
        if ioProcID != nil { AudioDeviceStop(aggregateDeviceID, ioProcID) }
    }

    private func destroyIOProc() {
        if let proc = ioProcID {
            AudioDeviceDestroyIOProcID(aggregateDeviceID, proc)
            ioProcID = nil
        }
    }

    private func destroyAggregateDevice() {
        if aggregateDeviceID != AudioObjectID(kAudioObjectUnknown) {
            AudioHardwareDestroyAggregateDevice(aggregateDeviceID)
            aggregateDeviceID = AudioObjectID(kAudioObjectUnknown)
        }
    }

    private func destroyTap() {
        if tapObjectID != AudioObjectID(kAudioObjectUnknown) {
            AudioHardwareDestroyProcessTap(tapObjectID)
            tapObjectID = AudioObjectID(kAudioObjectUnknown)
        }
    }
}

#else // !canImport(CoreAudio)

// Portable fallback so the app sources parse on non-Apple toolchains (e.g. Linux
// CI). All Core Audio behaviour is unavailable here.
enum ProcessTapError: Error {
    case processNotFound
    case outputDeviceUnavailable
    case tapCreationFailed(Int32)
    case aggregateDeviceCreationFailed(Int32)
    case ioProcCreationFailed(Int32)
    case formatUnavailable
}

final class ProcessTap {
    let bundleId: String
    let stripID: EngineBridge.StripID
    private(set) var sampleRate: Double = 0
    private(set) var channelCount: UInt32 = 0
    private(set) var isRunning = false

    init(bundleId: String, stripID: EngineBridge.StripID) {
        self.bundleId = bundleId
        self.stripID = stripID
    }

    func activate() throws { throw ProcessTapError.processNotFound }
    func tearDown() {}
    func takePeak() -> Float { 0 }
}

#endif
