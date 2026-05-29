//
//  EngineBridge.swift
//  SonicPatch
//
//  Thin Swift wrapper over the C++ `sonicpatch::AudioEngine` facade, reached via
//  Swift's C++ interoperability (build setting SWIFT_OBJC_INTEROP_MODE = objcxx,
//  i.e. Xcode "C++ / Objective-C++"; see project.yml and Docs/BUILD.md).
//
//  This is the *only* file that touches the C++ types directly. Everything else
//  in the app speaks the Swifty API below. The engine's public header
//  (SonicPatchEngine/Public/AudioEngine.hpp) is dependency-free PODs so it
//  imports cleanly into Swift.
//
//  NOTE: The C++ engine is not built in this (Linux) environment, so the calls
//  into `sonicpatch.AudioEngine` are shown as documented stubs / commented call
//  sites. On an Apple build with the framework linked, uncomment the interop
//  calls and remove the local fallbacks.
//

import Foundation

// On an Apple build with the SonicPatchEngine framework linked and C++ interop
// enabled, the C++ namespace is imported like a Swift module:
//
//     import SonicPatchEngine
//
// and `sonicpatch::AudioEngine` is usable as `sonicpatch.AudioEngine`.
// We guard the import so this file still parses where the framework is absent.
#if canImport(SonicPatchEngine)
import SonicPatchEngine
#endif

/// Swift-facing facade over the real-time C++ audio engine.
///
/// Method names follow Swift API design guidelines while mirroring the C++
/// contract 1:1. Parameter mutations are forwarded to the engine, which applies
/// them in a real-time-safe way on the audio thread.
final class EngineBridge {

    /// Mirrors `sonicpatch::StripID` (uint32_t; 0 == invalid).
    typealias StripID = UInt32

    /// Sentinel for "no strip", matching `sonicpatch::kInvalidStrip`.
    static let invalidStrip: StripID = 0

    // The owned C++ engine instance. With interop enabled this is:
    //     private var engine = sonicpatch.AudioEngine()
    // Held here for the lifetime of the bridge.
    //
    // #if canImport(SonicPatchEngine)
    // private var engine = sonicpatch.AudioEngine()
    // #endif

    /// Tracks running state locally so the UI has a value even in the stub build.
    private var running = false

    init() {
        // TODO(Phase 1): construct the C++ engine and call setAudioFormat with
        // the negotiated tap/device format before start().
    }

    // MARK: Lifecycle

    func start() {
        // TODO(Phase 1): engine.start()
        running = true
    }

    func stop() {
        // TODO(Phase 1): engine.stop()
        running = false
    }

    func isRunning() -> Bool {
        // TODO(Phase 1): return engine.isRunning()
        running
    }

    /// Reconfigure the engine's PCM format. Must be called while stopped.
    func setAudioFormat(sampleRate: Double, channels: UInt32, framesPerBuffer: UInt32) {
        // TODO(Phase 1):
        //   var fmt = sonicpatch.AudioFormat()
        //   fmt.sampleRate = sampleRate
        //   fmt.channels = channels
        //   fmt.framesPerBuffer = framesPerBuffer
        //   engine.setAudioFormat(fmt)
    }

    // MARK: Channel strips

    /// Create a strip capturing `bundleId`. Returns `invalidStrip` on failure.
    func createChannelStrip(bundleId: String) -> StripID {
        // TODO(Phase 1):
        //   return bundleId.withCString { engine.createChannelStrip($0) }
        Self.invalidStrip
    }

    func removeChannelStrip(_ strip: StripID) {
        // TODO(Phase 1): engine.removeChannelStrip(strip)
    }

    // MARK: Per-strip controls

    func setVolume(_ strip: StripID, db: Float) {
        // TODO(Phase 2): engine.setVolume(strip, db)
    }

    func setPan(_ strip: StripID, pan: Float) {
        // TODO(Phase 2): engine.setPan(strip, max(-1, min(1, pan)))
    }

    func setMute(_ strip: StripID, muted: Bool) {
        // TODO(Phase 2): engine.setMute(strip, muted)
    }

    func setInputTrim(_ strip: StripID, db: Float) {
        // TODO(Phase 2): engine.setInputTrim(strip, db)
    }

    /// Lock-free metering read.
    func getLevel(_ strip: StripID) -> LevelReading {
        // TODO(Phase 2):
        //   let snap = engine.getLevel(strip)
        //   return LevelReading(peak: snap.peak, rms: snap.rms)
        LevelReading()
    }

    // MARK: Built-in effects

    /// Insert a built-in DSP effect; returns an opaque slot handle (>= 0) or -1.
    @discardableResult
    func insertBuiltinEffect(_ strip: StripID,
                             slotIndex: Int,
                             type: BuiltinEffectKind) -> Int {
        // TODO(Phase 3):
        //   return Int(engine.insertBuiltinEffect(strip, Int32(slotIndex),
        //                                          type.cppValue))
        -1
    }

    func setEffectParameter(_ strip: StripID, slot: Int, paramId: UInt32, value: Float) {
        // TODO(Phase 3): engine.setEffectParameter(strip, Int32(slot), paramId, value)
    }

    func removeEffect(_ strip: StripID, slot: Int) {
        // TODO(Phase 3): engine.removeEffect(strip, Int32(slot))
    }
}

/// Swift mirror of `sonicpatch::BuiltinEffectType`. Kept as a separate Swift enum
/// so the rest of the app needn't import the C++ module; `cppValue` maps across
/// the interop boundary in `EngineBridge`.
enum BuiltinEffectKind: String, CaseIterable, Identifiable {
    case parametricEQ
    case compressor
    case limiter
    case noiseGate
    case highLowPassFilter
    case gainUtility

    var id: String { rawValue }

    var displayName: String {
        switch self {
        case .parametricEQ:       return "Parametric EQ"
        case .compressor:         return "Compressor"
        case .limiter:            return "Limiter"
        case .noiseGate:          return "Noise Gate"
        case .highLowPassFilter:  return "High/Low-pass Filter"
        case .gainUtility:        return "Gain Utility"
        }
    }

    // On an Apple build, map to the C++ enum:
    //
    // var cppValue: sonicpatch.BuiltinEffectType {
    //     switch self {
    //     case .parametricEQ:      return .ParametricEQ
    //     case .compressor:        return .Compressor
    //     case .limiter:           return .Limiter
    //     case .noiseGate:         return .NoiseGate
    //     case .highLowPassFilter: return .HighLowPassFilter
    //     case .gainUtility:       return .GainUtility
    //     }
    // }
}
