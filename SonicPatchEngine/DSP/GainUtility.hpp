#pragma once
//
// GainUtility.hpp
// A simple utility effect: gain (dB), constant-power pan, polarity invert, and
// optional mono-sum. All math implemented for real.
//
#include "IEffect.hpp"

namespace sonicpatch {

class GainUtility final : public IEffect {
public:
    enum Param : uint32_t {
        kGainDb   = 0,
        kPan      = 1, ///< -1..+1 (stereo only)
        kPolarity = 2, ///< >=0.5 => invert
        kMonoSum  = 3  ///< >=0.5 => sum all channels to mono
    };

    GainUtility() = default;

    void prepare(const AudioFormat& fmt) override;
    void reset() override;
    void process(float** io, int channels, int frames) override; // [RT]
    void setParameter(uint32_t id, float value) override;
    const char* name() const override { return "GainUtility"; }

private:
    float gainDb_   = 0.0f;
    float pan_      = 0.0f;
    bool  invert_   = false;
    bool  monoSum_  = false;
};

} // namespace sonicpatch
