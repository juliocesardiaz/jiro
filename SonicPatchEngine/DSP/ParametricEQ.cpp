//
// ParametricEQ.cpp
//
#include "ParametricEQ.hpp"

#include <algorithm>

namespace sonicpatch {

ParametricEQ::ParametricEQ() {
    // Sensible default band layout (flat: 0 dB gain everywhere).
    params_[0] = { BiquadType::LowShelf,   120.0,   0.0, 0.707 };
    params_[1] = { BiquadType::Peaking,    300.0,   0.0, 1.0   };
    params_[2] = { BiquadType::Peaking,   1000.0,   0.0, 1.0   };
    params_[3] = { BiquadType::Peaking,   3500.0,   0.0, 1.0   };
    params_[4] = { BiquadType::HighShelf, 10000.0,  0.0, 0.707 };
}

void ParametricEQ::prepare(const AudioFormat& fmt) {
    sampleRate_ = fmt.sampleRate;
    channels_   = std::min<int>(static_cast<int>(fmt.channels), kMaxChannels);

    for (int b = 0; b < kNumBands; ++b) {
        filters_[b].assign(static_cast<size_t>(channels_), Biquad{});
        recomputeBand(b);
    }
}

void ParametricEQ::reset() {
    for (auto& band : filters_) {
        for (auto& f : band) f.reset();
    }
}

void ParametricEQ::recomputeBand(int band) {
    const BandParams& p = params_[band];
    const BiquadCoeffs c = BiquadCoeffs::design(p.type, sampleRate_, p.freq, p.q, p.gainDb);
    for (Biquad& f : filters_[band]) {
        f.setCoeffs(c); // does not clear filter state, avoids clicks
    }
}

void ParametricEQ::setParameter(uint32_t id, float value) {
    const int band  = static_cast<int>(id / kParamsPerBand);
    const int field = static_cast<int>(id % kParamsPerBand);
    if (band < 0 || band >= kNumBands) return;

    switch (field) {
        case kFreq:   params_[band].freq   = value; break;
        case kGainDb: params_[band].gainDb = value; break;
        case kQ:      params_[band].q      = value; break;
        default: return;
    }
    // Recompute coefficients on the control thread only. process() never does.
    // TODO(Phase 2): publish coeffs to the audio thread via a double-buffered
    // atomic swap rather than mutating Biquad coeffs directly, to be strictly
    // race-free if EQ runs on the audio thread while this writes.
    recomputeBand(band);
}

void ParametricEQ::process(float** io, int channels, int frames) {
    // [RT] Cascade every band through every channel, in place.
    const int ch = std::min(channels, channels_);
    for (int b = 0; b < kNumBands; ++b) {
        std::vector<Biquad>& band = filters_[b];
        for (int c = 0; c < ch; ++c) {
            band[static_cast<size_t>(c)].processBlock(io[c], frames);
        }
    }
}

} // namespace sonicpatch
