//
// NoiseGate.cpp
//
#include "NoiseGate.hpp"
#include "DspMath.hpp"

#include <algorithm>
#include <cmath>

namespace sonicpatch {

using dsp::linearToDb;
using dsp::onePoleCoeffMs;

void NoiseGate::prepare(const AudioFormat& fmt) {
    sampleRate_ = fmt.sampleRate;
    recompute();
    reset();
}

void NoiseGate::reset() {
    envGain_   = 0.0f;
    holdCount_ = 0;
}

void NoiseGate::recompute() {
    attackCoeff_  = onePoleCoeffMs(attackMs_,  sampleRate_);
    releaseCoeff_ = onePoleCoeffMs(releaseMs_, sampleRate_);
    holdSamples_  = static_cast<int>((holdMs_ * 0.001) * sampleRate_);
}

void NoiseGate::setParameter(uint32_t id, float value) {
    switch (id) {
        case kThresholdDb: thresholdDb_ = value; break;
        case kAttackMs:    attackMs_    = value; recompute(); break;
        case kReleaseMs:   releaseMs_   = value; recompute(); break;
        case kHoldMs:      holdMs_      = value; recompute(); break;
        case kRangeDb:     rangeDb_     = value; break;
        default: break;
    }
}

void NoiseGate::process(float** io, int channels, int frames) {
    // [RT] level-detected gate with attack/release/hold smoothing.
    // TODO(Phase 3): add hysteresis (open vs close thresholds) and apply rangeDb_
    // as the closed-gate floor instead of full mute.
    for (int n = 0; n < frames; ++n) {
        float peak = 0.0f;
        for (int c = 0; c < channels; ++c) {
            peak = std::max(peak, std::fabs(io[c][n]));
        }
        const bool open = linearToDb(peak) > thresholdDb_;

        float target;
        if (open) {
            target = 1.0f;
            holdCount_ = holdSamples_;
        } else if (holdCount_ > 0) {
            --holdCount_;
            target = 1.0f; // hold open
        } else {
            target = 0.0f; // closed
        }

        const float coeff = (target > envGain_) ? attackCoeff_ : releaseCoeff_;
        envGain_ = coeff * envGain_ + (1.0f - coeff) * target;

        for (int c = 0; c < channels; ++c) {
            io[c][n] *= envGain_;
        }
    }
}

} // namespace sonicpatch
