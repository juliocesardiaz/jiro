//
// Compressor.cpp
//
#include "Compressor.hpp"
#include "DspMath.hpp"

#include <algorithm>
#include <cmath>

namespace sonicpatch {

using dsp::dbToLinear;
using dsp::linearToDb;
using dsp::onePoleCoeffMs;

void Compressor::prepare(const AudioFormat& fmt) {
    sampleRate_ = fmt.sampleRate;
    recomputeCoeffs();
    reset();
}

void Compressor::reset() {
    gainReductionDb_ = 0.0f;
}

void Compressor::recomputeCoeffs() {
    attackCoeff_  = onePoleCoeffMs(attackMs_,  sampleRate_);
    releaseCoeff_ = onePoleCoeffMs(releaseMs_, sampleRate_);
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
    const float makeupLin = dbToLinear(makeupDb_);
    const float invRatio  = 1.0f / ratio_;

    for (int n = 0; n < frames; ++n) {
        // 1. Detect peak across channels for this sample.
        float peak = 0.0f;
        for (int c = 0; c < channels; ++c) {
            peak = std::max(peak, std::fabs(io[c][n]));
        }
        const float inDb = linearToDb(peak);

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
        const float gain = dbToLinear(gainReductionDb_) * makeupLin;
        for (int c = 0; c < channels; ++c) {
            io[c][n] *= gain;
        }
    }
}

} // namespace sonicpatch
