#pragma once
//
// ParametricEQ.hpp
// A 5-band parametric equaliser: low shelf + 3 peaking bands + high shelf.
//
// Each band is a cascaded Biquad per channel. Coefficients are recomputed only
// when a parameter changes (control thread), never per-sample, so process() is
// just a chain of biquad evaluations.
//
#include "IEffect.hpp"
#include "Biquad.hpp"

#include <array>
#include <vector>

namespace sonicpatch {

class ParametricEQ final : public IEffect {
public:
    static constexpr int kNumBands    = 5;   ///< [0]=lowshelf [1..3]=peak [4]=highshelf
    static constexpr int kMaxChannels = 8;

    /// Parameter ids. Encoding: paramId = band * kParamsPerBand + field.
    enum Field : uint32_t { kFreq = 0, kGainDb = 1, kQ = 2, kParamsPerBand = 3 };

    ParametricEQ();

    void prepare(const AudioFormat& fmt) override;
    void reset() override;
    void process(float** io, int channels, int frames) override; // [RT]
    void setParameter(uint32_t id, float value) override;
    const char* name() const override { return "ParametricEQ"; }

private:
    struct BandParams {
        BiquadType type;
        double     freq;
        double     gainDb;
        double     q;
    };

    void recomputeBand(int band);

    double sampleRate_ = 48000.0;
    int    channels_   = 2;

    std::array<BandParams, kNumBands> params_;
    // [band][channel] biquads. Sized in prepare(); not resized in process().
    std::array<std::vector<Biquad>, kNumBands> filters_;
};

} // namespace sonicpatch
