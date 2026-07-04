//
// GainUtility.cpp
//
#include "GainUtility.hpp"
#include "DspMath.hpp"

#include <cmath>

namespace sonicpatch {

void GainUtility::prepare(const AudioFormat&) {}
void GainUtility::reset() {}

void GainUtility::setParameter(uint32_t id, float value) {
    switch (id) {
        case kGainDb:   gainDb_  = value; break;
        case kPan:      pan_     = value; break;
        case kPolarity: invert_  = value >= 0.5f; break;
        case kMonoSum:  monoSum_ = value >= 0.5f; break;
        default: break;
    }
}

void GainUtility::process(float** io, int channels, int frames) {
    // [RT]
    const float gain = dsp::dbToLinear(gainDb_) * (invert_ ? -1.0f : 1.0f);

    // Optional mono sum (averaged to preserve level), applied first.
    if (monoSum_ && channels > 1) {
        for (int n = 0; n < frames; ++n) {
            float sum = 0.0f;
            for (int c = 0; c < channels; ++c) sum += io[c][n];
            const float mono = sum / static_cast<float>(channels);
            for (int c = 0; c < channels; ++c) io[c][n] = mono;
        }
    }

    // Constant-power pan for stereo: theta in [0, pi/2].
    if (channels == 2) {
        float panL = 1.0f, panR = 1.0f;
        dsp::constantPowerPanGains(pan_, panL, panR);
        const float lGain = panL * gain;
        const float rGain = panR * gain;
        for (int n = 0; n < frames; ++n) {
            io[0][n] *= lGain;
            io[1][n] *= rGain;
        }
    } else {
        for (int c = 0; c < channels; ++c) {
            for (int n = 0; n < frames; ++n) io[c][n] *= gain;
        }
    }
}

} // namespace sonicpatch
