//
// Limiter.cpp
//
#include "Limiter.hpp"

#include <algorithm>
#include <cmath>

namespace sonicpatch {

namespace {
inline float dbToLin(float db) { return std::pow(10.0f, db * 0.05f); }
inline float timeCoeff(float ms, double sr) {
    if (ms <= 0.0f) return 0.0f;
    return static_cast<float>(std::exp(-1.0 / ((ms * 0.001) * sr)));
}
} // namespace

void Limiter::prepare(const AudioFormat& fmt) {
    sampleRate_ = fmt.sampleRate;
    recompute();
    reset();
    // TODO(Phase 3): allocate the look-ahead delay line:
    //   delaySamples = ceil(lookaheadMs_ * 0.001 * sampleRate_), one ring per ch.
}

void Limiter::reset() {
    envGain_ = 1.0f;
}

void Limiter::recompute() {
    releaseCoeff_ = timeCoeff(releaseMs_, sampleRate_);
}

void Limiter::setParameter(uint32_t id, float value) {
    switch (id) {
        case kCeilingDb:   ceilingDb_   = value; break;
        case kReleaseMs:   releaseMs_   = value; recompute(); break;
        case kLookaheadMs: lookaheadMs_ = std::max(0.0f, value); break;
        default: break;
    }
}

void Limiter::process(float** io, int channels, int frames) {
    // [RT] Working (zero-latency) peak limiter. Instant attack, smoothed release.
    // TODO(Phase 3): feed samples through the look-ahead delay and derive the
    // gain from the *future* peak so the ceiling is never exceeded transiently.
    const float ceilingLin = dbToLin(ceilingDb_);

    for (int n = 0; n < frames; ++n) {
        float peak = 0.0f;
        for (int c = 0; c < channels; ++c) {
            peak = std::max(peak, std::fabs(io[c][n]));
        }

        // Required gain to keep this sample under the ceiling.
        float targetGain = 1.0f;
        if (peak > ceilingLin && peak > 0.0f) {
            targetGain = ceilingLin / peak;
        }

        // Instant attack (clamp down immediately), smoothed release upward.
        if (targetGain < envGain_) {
            envGain_ = targetGain;
        } else {
            envGain_ = releaseCoeff_ * envGain_ + (1.0f - releaseCoeff_) * targetGain;
        }

        for (int c = 0; c < channels; ++c) {
            io[c][n] *= envGain_;
        }
    }
}

} // namespace sonicpatch
