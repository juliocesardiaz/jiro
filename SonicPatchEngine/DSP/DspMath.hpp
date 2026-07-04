#pragma once
//
// DspMath.hpp
// The single home for small DSP math shared across effects and nodes.
//
// These are BEHAVIORAL constants, not boilerplate: the -120 dB floor, the
// one-pole time-constant formula, and the constant-power pan law define how
// SonicPatch sounds. Keeping one copy means the compressor's detector, the
// gate's detector, the fader, and the utility pan can never silently diverge.
//
// Everything here is header-only, allocation-free, and safe on the audio
// thread.
//
#include <cmath>
#include <cstdint>

namespace sonicpatch {
namespace dsp {

constexpr float kPi = 3.14159265358979323846f;

/// dB floor used when converting near-zero linear values (avoids log(0)).
constexpr float kDbFloor = -120.0f;

/// Linear amplitude -> dBFS, floored at kDbFloor.
inline float linearToDb(float lin) noexcept {
    return lin > 1e-6f ? 20.0f * std::log10(lin) : kDbFloor;
}

/// dB -> linear amplitude.
inline float dbToLinear(float db) noexcept {
    return std::pow(10.0f, db * 0.05f);
}

/// One-pole smoothing coefficient for a time constant given in milliseconds:
/// coeff = exp(-1 / (tau * fs)). Returns 0 (instant) for ms <= 0.
inline float onePoleCoeffMs(float ms, double sampleRate) noexcept {
    if (ms <= 0.0f) return 0.0f;
    return static_cast<float>(std::exp(-1.0 / ((ms * 0.001) * sampleRate)));
}

/// One-pole smoothing coefficient for a time constant given in seconds.
inline float onePoleCoeff(double seconds, double sampleRate) noexcept {
    if (seconds <= 0.0) return 0.0f;
    return static_cast<float>(std::exp(-1.0 / (seconds * sampleRate)));
}

/// Advance a one-pole smoother one step toward `target`. [RT]
inline float smoothToward(float current, float target, float coeff) noexcept {
    return coeff * current + (1.0f - coeff) * target;
}

/// Constant-power stereo pan law. pan in [-1, +1] maps to a quarter circle:
/// left = cos(theta), right = sin(theta), theta = (pan/2 + 0.5) * pi/2.
/// Equal power at every position; -3 dB per side at center.
inline void constantPowerPanGains(float pan, float& left, float& right) noexcept {
    const float theta = (pan * 0.5f + 0.5f) * (kPi * 0.5f);
    left  = std::cos(theta);
    right = std::sin(theta);
}

/// Largest absolute sample across `channels` channel pointers at frame `n`.
/// The shared sidechain detector scan used by the dynamics effects. [RT]
inline float peakAcrossChannels(const float* const* io, int channels, int n) noexcept {
    float peak = 0.0f;
    for (int c = 0; c < channels; ++c) {
        const float v = std::fabs(io[c][n]);
        if (v > peak) peak = v;
    }
    return peak;
}

} // namespace dsp
} // namespace sonicpatch
