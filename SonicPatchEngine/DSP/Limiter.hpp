#pragma once
//
// Limiter.hpp
// Brick-wall look-ahead peak limiter (skeleton).
//
// Design (to be completed in Phase 3):
//   * A short look-ahead delay line (e.g. 1.5 ms) lets the gain envelope react
//     before the peak it is taming actually reaches the output, giving a true
//     brick-wall ceiling with no overshoot.
//   * The detector computes the required attenuation from the delayed peak; an
//     instantaneous attack (look-ahead absorbs the transient) plus a smooth
//     release avoids pumping.
//
// This skeleton implements a working (non-look-ahead) hard ceiling so the chain
// is audible and testable; the look-ahead delay line is the documented TODO.
//
#include "IEffect.hpp"

namespace sonicpatch {

class Limiter final : public IEffect {
public:
    enum Param : uint32_t {
        kCeilingDb   = 0, ///< output ceiling in dBFS (e.g. -1.0)
        kReleaseMs   = 1,
        kLookaheadMs = 2  ///< TODO(Phase 3): wire to delay-line length
    };

    Limiter() = default;

    void prepare(const AudioFormat& fmt) override;
    void reset() override;
    void process(float** io, int channels, int frames) override; // [RT]
    void setParameter(uint32_t id, float value) override;
    const char* name() const override { return "Limiter"; }

private:
    double sampleRate_ = 48000.0;
    float  ceilingDb_  = -1.0f;
    float  releaseMs_  = 50.0f;
    float  lookaheadMs_ = 1.5f;

    float  releaseCoeff_ = 0.0f;
    float  envGain_      = 1.0f; ///< current gain (1.0 = unity)

    void recompute();
};

} // namespace sonicpatch
