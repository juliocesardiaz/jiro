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
//  All engine calls below are LIVE code, conditionally compiled: on an Apple
//  build with the SonicPatchEngine framework linked they call straight into
//  the C++ facade; on toolchains without the framework (e.g. Linux CI, which
//  only parses these sources) the `#if canImport(SonicPatchEngine)` guards
//  fall back to inert stubs. There is nothing to uncomment.
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

    // The owned C++ engine instance, reached through Swift C++ interop.
    //
    // `sonicpatch::AudioEngine` is a *copyable shared handle* (its state lives
    // behind a shared Impl), specifically so Swift 5.9 — which cannot import
    // move-only C++ types — can store it as a stored property. Copies alias
    // the same engine.
    #if canImport(SonicPatchEngine)
    private var engine = sonicpatch.AudioEngine()
    #endif

    /// Tracks running state locally so the UI has a value even in the stub build.
    private var running = false

    init() {
        // The negotiated tap/device format is applied (while stopped) before
        // start(); see setAudioFormat(). A sensible default is set here.
        setAudioFormat(sampleRate: 48_000, channels: 2, framesPerBuffer: 512)
    }

    // MARK: Lifecycle

    func start() {
        #if canImport(SonicPatchEngine)
        engine.start()
        #endif
        running = true
    }

    func stop() {
        #if canImport(SonicPatchEngine)
        engine.stop()
        #endif
        running = false
    }

    func isRunning() -> Bool {
        #if canImport(SonicPatchEngine)
        return engine.isRunning()
        #else
        return running
        #endif
    }

    /// Reconfigure the engine's PCM format. Must be called while stopped.
    func setAudioFormat(sampleRate: Double, channels: UInt32, framesPerBuffer: UInt32) {
        #if canImport(SonicPatchEngine)
        var fmt = sonicpatch.AudioFormat()
        fmt.sampleRate      = sampleRate
        fmt.channels        = channels
        fmt.framesPerBuffer = framesPerBuffer
        engine.setAudioFormat(fmt)
        #endif
    }

    // MARK: Channel strips

    /// Create a strip capturing `bundleId`. Returns `invalidStrip` on failure.
    func createChannelStrip(bundleId: String) -> StripID {
        #if canImport(SonicPatchEngine)
        return bundleId.withCString { engine.createChannelStrip($0) }
        #else
        return Self.invalidStrip
        #endif
    }

    func removeChannelStrip(_ strip: StripID) {
        #if canImport(SonicPatchEngine)
        engine.removeChannelStrip(strip)
        #endif
    }

    // MARK: Per-strip controls

    func setVolume(_ strip: StripID, db: Float) {
        #if canImport(SonicPatchEngine)
        engine.setVolume(strip, db)
        #endif
    }

    func setPan(_ strip: StripID, pan: Float) {
        #if canImport(SonicPatchEngine)
        engine.setPan(strip, max(-1, min(1, pan)))
        #endif
    }

    func setMute(_ strip: StripID, muted: Bool) {
        #if canImport(SonicPatchEngine)
        engine.setMute(strip, muted)
        #endif
    }

    func setInputTrim(_ strip: StripID, db: Float) {
        #if canImport(SonicPatchEngine)
        engine.setInputTrim(strip, db)
        #endif
    }

    /// Lock-free metering read.
    func getLevel(_ strip: StripID) -> LevelReading {
        #if canImport(SonicPatchEngine)
        let snap = engine.getLevel(strip)
        return LevelReading(peak: snap.peak, rms: snap.rms)
        #else
        return LevelReading()
        #endif
    }

    // MARK: Built-in effects

    /// Insert a built-in DSP effect; returns an opaque slot handle (>= 0) or -1.
    @discardableResult
    func insertBuiltinEffect(_ strip: StripID,
                             slotIndex: Int,
                             type: BuiltinEffectKind) -> Int {
        #if canImport(SonicPatchEngine)
        return Int(engine.insertBuiltinEffect(strip, Int32(slotIndex), type.cppValue))
        #else
        return -1
        #endif
    }

    func setEffectParameter(_ strip: StripID, slot: Int, paramId: UInt32, value: Float) {
        #if canImport(SonicPatchEngine)
        engine.setEffectParameter(strip, Int32(slot), paramId, value)
        #endif
    }

    func removeEffect(_ strip: StripID, slot: Int) {
        #if canImport(SonicPatchEngine)
        engine.removeEffect(strip, Int32(slot))
        #endif
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
}

#if canImport(SonicPatchEngine)
extension BuiltinEffectKind {
    /// Maps to the C++ `sonicpatch::BuiltinEffectType` across the interop boundary.
    var cppValue: sonicpatch.BuiltinEffectType {
        switch self {
        case .parametricEQ:      return .ParametricEQ
        case .compressor:        return .Compressor
        case .limiter:           return .Limiter
        case .noiseGate:         return .NoiseGate
        case .highLowPassFilter: return .HighLowPassFilter
        case .gainUtility:       return .GainUtility
        }
    }
}
#endif
