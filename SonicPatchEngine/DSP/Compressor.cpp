//
// Compressor.cpp
//
#include "Compressor.hpp"

#include <algorithm>
#include <cmath>

namespace sonicpatch {

namespace {
inline float linToDb(float lin) {
    // -120 dBFS floor to avoid log(0).
    return lin > 1e-6f ? 20.0f * std::log10(lin) : -120.0f;
}
inline float dbToLin(float db) {
    return std::pow(10.0f, db * 0.05f);
}
// One-pole time-constant coefficient for a given time in ms.
inline float timeCoeff(float ms, double sr) {
    if (ms <= 0.0f) return 0.0f;
    return static_cast<float>(std::exp(-1.0 / ((ms * 0.001) * sr)));
}
} // namespace

void Compressor::prepare(const AudioFormat& fmt) {
    sampleRate_ = fmt.sampleRate;
    recomputeCoeffs();
    reset();
}

void Compressor::reset() {
    gainReductionDb_ = 0.0f;
}

void Compressor::recomputeCoeffs() {
    attackCoeff_  = timeCoeff(attackMs_,  sampleRate_);
    releaseCoeff_ = timeCoeff(releaseMs_, sampleRate_);
}

void Compressor::setParameter(uint32_t id, float value) {
    switch (id) {
        case kThresholdDb: thresholdDb_ = value; break;
        case kRatio:       ratio_       = std::max(1.0f, value); break;
        case kAttackMs:    attackMs_    = value; recomputeCoeffs(); break;
        case kReleaseMs:   releaseMs_   = value; recomputeCoeffs(); break;
        case kMakeupDb:    makeupDb_    = value; break;
        case kKneeDb:      kneeDb_      = std::max(0.0f, value); break;
        default: break;
    }
}

void Compressor::process(float** io, int channels, int frames) {
    // [RT] feed-forward, sample-accurate gain smoothing.
    const float makeupLin = dbToLin(makeupDb_);
    const float invRatio  = 1.0f / ratio_;

    for (int n = 0; n < frames; ++n) {
        // 1. Detect peak across channels for this sample.
        float peak = 0.0f;
        for (int c = 0; c < channels; ++c) {
            peak = std::max(peak, std::fabs(io[c][n]));
        }
        const float inDb = linToDb(peak);

        // 2. Static gain computer (hard knee for now).
        // TODO(Phase 3): soft knee using kneeDb_ for a smooth transition region.
        float targetReductionDb = 0.0f;
        if (inDb > thresholdDb_) {
            const float over = inDb - thresholdDb_;
            // Output rises at 1/ratio above threshold; reduction is the deficit.
            targetReductionDb = -(over - over * invRatio);
        }

        // 3. Attack/release ballistics on the gain-reduction envelope.
        // More reduction (more negative) => attack; less => release.
        const float coeff = (targetReductionDb < gainReductionDb_)
                                ? attackCoeff_ : releaseCoeff_;
        gainReductionDb_ = coeff * gainReductionDb_ + (1.0f - coeff) * targetReductionDb;

        // 4. Apply makeup + smoothed gain reduction to all channels.
        const float gain = dbToLin(gainReductionDb_) * makeupLin;
        for (int c = 0; c < channels; ++c) {
            io[c][n] *= gain;
        }
    }
}

} // namespace sonicpatch
