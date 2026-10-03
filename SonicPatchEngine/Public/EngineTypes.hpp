#pragma once
//
// EngineTypes.hpp
// SonicPatch shared interface contract — fundamental value types used across
// the public facade and (transitively) the Swift C++ interop layer.
//
// This header is intentionally dependency-free: no Core Audio, no STL containers
// in the public surface, only fixed-width integers and trivially-copyable PODs.
// That keeps it cheap to include and clean to import into Swift.
//
#include <cstdint>

namespace sonicpatch {

/// Stable identifier for a node inside the audio graph.
using NodeID = uint32_t;

/// Stable identifier for a "channel strip" (a user-facing lane composed of
/// several internal graph nodes: input trim -> effects -> volume/pan -> meter).
using StripID = uint32_t;

/// Sentinel value indicating "no strip". Valid strips are always non-zero.
constexpr StripID kInvalidStrip = 0;

/// Describes the PCM format the engine renders at. Non-interleaved float32 is
/// assumed throughout the engine; this struct only carries the rate/geometry.
struct AudioFormat {
    double   sampleRate     = 48000.0;
    uint32_t channels       = 2;
    uint32_t framesPerBuffer = 512;
};

/// Kinds of nodes the graph can contain. The public API exposes channel strips
/// rather than raw nodes, but these are surfaced for diagnostics/tooling.
enum class NodeKind {
    TapSource,   ///< Pulls audio from a Core Audio process/aggregate tap.
    DeviceSink,  ///< Pushes audio to an output device IOProc.
    Mixer,       ///< Sums N inputs into one output.
    Splitter,    ///< Fans one input out to N outputs.
    Volume,      ///< dB gain + constant-power pan + mute.
    Meter,       ///< Peak/RMS metering tap (pass-through).
    Effect       ///< Builtin DSP or hosted AudioUnit.
};

/// The builtin (first-party, no AU hosting required) effect catalogue.
enum class BuiltinEffectType {
    ParametricEQ,
    Compressor,
    Limiter,
    NoiseGate,
    HighLowPassFilter,
    GainUtility
};

/// A lock-free metering readout. Produced on the audio thread, consumed on the
/// UI thread via atomics (see RtSafe.hpp). Values are linear, not dB.
struct LevelSnapshot {
    float peak = 0.0f;
    float rms  = 0.0f;
};

} // namespace sonicpatch
