//
// Filters.cpp
//
#include "Filters.hpp"

#include <algorithm>

namespace sonicpatch {

void Filters::prepare(const AudioFormat& fmt) {
    sampleRate_ = fmt.sampleRate;
    channels_   = static_cast<int>(fmt.channels);
    filters_.assign(static_cast<size_t>(channels_), Biquad{});
    recompute();
}

void Filters::reset() {
    for (auto& f : filters_) f.reset();
}

void Filters::recompute() {
    const BiquadType t = (mode_ == HighPass) ? BiquadType::HighPass : BiquadType::LowPass;
    const BiquadCoeffs c = BiquadCoeffs::design(t, sampleRate_, cutoff_, q_);
    for (Biquad& f : filters_) f.setCoeffs(c);
}

void Filters::setParameter(uint32_t id, float value) {
    switch (id) {
        case kMode:   mode_   = (value >= 0.5f) ? HighPass : LowPass; break;
        case kCutoff: cutoff_ = value; break;
        case kQ:      q_      = value; break;
        default: return;
    }
    recompute();
}

void Filters::process(float** io, int channels, int frames) {
    // [RT] one biquad per channel.
    const int ch = std::min(channels, channels_);
    for (int c = 0; c < ch; ++c) {
        filters_[static_cast<size_t>(c)].processBlock(io[c], frames);
    }
}

} // namespace sonicpatch
