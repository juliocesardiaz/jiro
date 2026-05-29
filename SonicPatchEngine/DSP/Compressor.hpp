#pragma once
//
// Compressor.hpp
// Feed-forward dynamics compressor with a peak/RMS envelope follower.
//
// Signal path (per block, all channels share one gain-reduction envelope so the
// stereo image is preserved):
//   1. detect level (max across channels), convert to dB
//   2. apply static gain-computer (threshold/ratio/knee) -> target gain dB
//   3. smooth gain dB with attack/release one-pole ballistics
//   4. apply makeup + smoothed gain to all channels
//
// The attack/release smoothing and the gain computer are implemented for real.
// Soft-knee and sidechain filtering are left as documented TODOs.
//
#include "IEffect.hpp"

namespace sonicpatch {

class Compressor final : public IEffect {
public:
    enum Param : uint32_t {
        kThresholdDb = 0, ///< dBFS, e.g. -18
        kRatio       = 1, ///< >= 1.0, e.g. 4 means 4:1
        kAttackMs    = 2,
        kReleaseMs   = 3,
        kMakeupDb    = 4,
        kKneeDb      = 5  ///< soft-knee width (TODO: full soft knee)
    };

    Compressor() = default;

    void prepare(const AudioFormat& fmt) override;
    void reset() override;
    void process(float** io, int channels, int frames) override; // [RT]
    void setParameter(uint32_t id, float value) override;
    const char* name() const override { return "Compressor"; }

private:
    void recomputeCoeffs();

    double sampleRate_ = 48000.0;

    // Parameters (control thread writes, audio thread reads; plain floats are
    // fine here because tearing of a single 32-bit value is benign for these).
    float thresholdDb_ = -18.0f;
    float ratio_       = 4.0f;
    float attackMs_    = 10.0f;
    float releaseMs_   = 100.0f;
    float makeupDb_    = 0.0f;
    float kneeDb_      = 6.0f;

    // Derived one-pole coefficients (per-sample smoothing).
    float attackCoeff_  = 0.0f;
    float releaseCoeff_ = 0.0f;

    // Envelope state: current gain reduction in dB (<= 0).
    float gainReductionDb_ = 0.0f;
};

} // namespace sonicpatch
