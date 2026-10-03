#pragma once
//
// Filters.hpp
// Simple high-pass / low-pass utility filter (12 dB/oct) built on Biquad.
//
// A single 2nd-order Butterworth-Q (0.707) biquad per channel gives the classic
// 12 dB/octave rolloff. The mode selects HP or LP; both share one cutoff. This
// is the "HighLowPassFilter" builtin effect.
//
#include "IEffect.hpp"
#include "Biquad.hpp"

#include <vector>

namespace sonicpatch {

class Filters final : public IEffect {
public:
    enum Mode : int { LowPass = 0, HighPass = 1 };

    enum Param : uint32_t {
        kMode    = 0, ///< 0 = LP, 1 = HP
        kCutoff  = 1, ///< Hz
        kQ       = 2
    };

    Filters() = default;

    void prepare(const AudioFormat& fmt) override;
    void reset() override;
    void process(float** io, int channels, int frames) override; // [RT]
    void setParameter(uint32_t id, float value) override;
    const char* name() const override { return "HighLowPassFilter"; }

private:
    void recompute();

    double sampleRate_ = 48000.0;
    int    channels_   = 2;
    Mode   mode_       = LowPass;
    double cutoff_     = 1000.0;
    double q_          = 0.707;

    std::vector<Biquad> filters_; ///< one per channel; sized in prepare()
};

} // namespace sonicpatch
