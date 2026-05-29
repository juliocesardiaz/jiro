//
//  ProcessTap.swift
//  SonicPatch
//
//  Wraps a single Core Audio process tap and its private aggregate device.
//
//  Lifecycle (Phase 1 centerpiece):
//    1. Resolve the target process' AudioObjectID(s).
//    2. Build a CATapDescription (mono/stereo mixdown of those processes).
//    3. AudioHardwareCreateProcessTap -> tap object id.
//    4. Read the tap's stream format via kAudioTapPropertyFormat.
//    5. Create a *private* aggregate device whose tap list is this tap
//       (kAudioAggregateDeviceTapListKey, kAudioAggregateDeviceIsPrivateKey).
//    6. Install an IOProc via AudioDeviceCreateIOProcIDWithBlock and start IO.
//    7. On teardown, stop/destroy IOProc, aggregate device, and the tap.
//
//  The Core Audio calls are stubbed here (no Core Audio in this environment) but
//  the signatures, ordering, and cleanup are accurate. Replace the stub bodies
//  with the real calls on an Apple build.
//

import Foundation
#if canImport(CoreAudio)
import CoreAudio
import AudioToolbox
#endif

/// Errors that can occur while creating or running a tap.
enum ProcessTapError: Error {
    case processNotFound
    case tapCreationFailed(OSStatusValue)
    case aggregateDeviceCreationFailed(OSStatusValue)
    case ioProcCreationFailed(OSStatusValue)
    case formatUnavailable
}

/// Aliased so this file parses without CoreAudio (where `OSStatus` is undefined).
#if canImport(CoreAudio)
typealias OSStatusValue = OSStatus
#else
typealias OSStatusValue = Int32
#endif

/// Owns one process tap and its private aggregate device, delivering captured
/// audio to the engine for a specific strip.
final class ProcessTap {

    let bundleId: String
    let stripID: EngineBridge.StripID

    /// Negotiated tap format (channels/rate), read from kAudioTapPropertyFormat.
    private(set) var sampleRate: Double = 0
    private(set) var channelCount: UInt32 = 0

    // Core Audio resource ids. Typed as UInt32 (AudioObjectID) so the file parses
    // without CoreAudio; real builds use AudioObjectID / AudioDeviceIOProcID.
    private var tapObjectID: UInt32 = 0
    private var aggregateDeviceID: UInt32 = 0
    private var ioProcID: UnsafeMutableRawPointer?   // AudioDeviceIOProcID (opaque)

    private(set) var isRunning = false

    init(bundleId: String, stripID: EngineBridge.StripID) {
        self.bundleId = bundleId
        self.stripID = stripID
    }

    deinit { tearDown() }

    // MARK: Activation

    /// Build the tap + aggregate device and start IO. Throws on any CA failure.
    func activate() throws {
        let processObject = try resolveProcessObject(forBundleId: bundleId)
        try createTap(forProcess: processObject)
        try readTapFormat()
        try createPrivateAggregateDevice()
        try installIOProc()
        try startIO()
        isRunning = true
    }

    /// Stop IO and release all Core Audio resources. Idempotent.
    func tearDown() {
        guard tapObjectID != 0 || aggregateDeviceID != 0 else { return }
        stopIO()
        destroyIOProc()
        destroyAggregateDevice()
        destroyTap()
        isRunning = false
    }

    // MARK: Steps (stubbed Core Audio)

    /// Resolve the AudioObjectID of the audio process for `bundleId`.
    private func resolveProcessObject(forBundleId bundleId: String) throws -> UInt32 {
        // TODO(Phase 1): enumerate kAudioHardwarePropertyProcessObjectList and
        // match kAudioProcessPropertyBundleID == bundleId. Throw .processNotFound
        // if absent (the caller defers and retries when the app becomes audible).
        throw ProcessTapError.processNotFound
    }

    /// Create the process tap from a CATapDescription.
    private func createTap(forProcess processObject: UInt32) throws {
        // TODO(Phase 1):
        //   let desc = CATapDescription(stereoMixdownOfProcesses: [processObject])
        //   // or CATapDescription(processes:) for a multi-channel tap.
        //   desc.isPrivate = true
        //   var tap = AudioObjectID(0)
        //   let status = AudioHardwareCreateProcessTap(desc, &tap)
        //   guard status == noErr else { throw .tapCreationFailed(status) }
        //   self.tapObjectID = tap
        throw ProcessTapError.tapCreationFailed(-1)
    }

    /// Read the tap's stream format (kAudioTapPropertyFormat) to learn
    /// rate/channel count before building the aggregate device.
    private func readTapFormat() throws {
        // TODO(Phase 1):
        //   var addr = AudioObjectPropertyAddress(
        //       mSelector: kAudioTapPropertyFormat,
        //       mScope: kAudioObjectPropertyScopeGlobal,
        //       mElement: kAudioObjectPropertyElementMain)
        //   var asbd = AudioStreamBasicDescription()
        //   var size = UInt32(MemoryLayout<AudioStreamBasicDescription>.size)
        //   let status = AudioObjectGetPropertyData(tapObjectID, &addr, 0, nil,
        //                                           &size, &asbd)
        //   guard status == noErr else { throw .formatUnavailable }
        //   self.sampleRate = asbd.mSampleRate
        //   self.channelCount = asbd.mChannelsPerFrame
        throw ProcessTapError.formatUnavailable
    }

    /// Create a *private* aggregate device whose tap list is our tap.
    private func createPrivateAggregateDevice() throws {
        // TODO(Phase 1): build the aggregate description dictionary with:
        //   kAudioAggregateDeviceUIDKey            = "SonicPatch.tap.<bundleId>"
        //   kAudioAggregateDeviceIsPrivateKey      = true   (not user-visible)
        //   kAudioAggregateDeviceIsStackedKey      = false
        //   kAudioAggregateDeviceTapListKey        = [ { tap UID } ]
        //   kAudioAggregateDeviceTapAutoStartKey   = true
        // then:
        //   var device = AudioObjectID(0)
        //   let status = AudioHardwareCreateAggregateDevice(dict as CFDictionary,
        //                                                   &device)
        //   guard status == noErr else { throw .aggregateDeviceCreationFailed(status) }
        //   self.aggregateDeviceID = device
        throw ProcessTapError.aggregateDeviceCreationFailed(-1)
    }

    /// Install the IOProc that delivers captured frames to the engine.
    private func installIOProc() throws {
        // TODO(Phase 1):
        //   var procID: AudioDeviceIOProcID?
        //   let status = AudioDeviceCreateIOProcIDWithBlock(
        //       &procID, aggregateDeviceID, /*dispatchQueue:*/ nil) {
        //       _, inInputData, _, _, _ in
        //       // RT-SAFE: copy/forward inInputData buffers into the engine's
        //       // TapSource for self.stripID. No allocation, no locks here.
        //   }
        //   guard status == noErr, let procID else { throw .ioProcCreationFailed(status) }
        //   self.ioProcID = UnsafeMutableRawPointer(procID)
        throw ProcessTapError.ioProcCreationFailed(-1)
    }

    private func startIO() throws {
        // TODO(Phase 1): AudioDeviceStart(aggregateDeviceID, ioProcID)
    }

    // MARK: Teardown steps

    private func stopIO() {
        // TODO(Phase 1): AudioDeviceStop(aggregateDeviceID, ioProcID)
    }

    private func destroyIOProc() {
        // TODO(Phase 1): AudioDeviceDestroyIOProcID(aggregateDeviceID, ioProcID)
        ioProcID = nil
    }

    private func destroyAggregateDevice() {
        // TODO(Phase 1): AudioHardwareDestroyAggregateDevice(aggregateDeviceID)
        aggregateDeviceID = 0
    }

    private func destroyTap() {
        // TODO(Phase 1): AudioHardwareDestroyProcessTap(tapObjectID)
        tapObjectID = 0
    }
}
