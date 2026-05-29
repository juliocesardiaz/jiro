#pragma once
//
// NoiseGate.hpp
// Downward expander / noise gate (skeleton with a working hard gate).
//
// When the detected level falls below the threshold the gate attenuates the
// signal; above the threshold it passes through. Attack opens the gate, release
// closes it, with a hold time keeping it open briefly to avoid chattering.
//
// This skeleton implements a functional level-detected gate with attack/release
// smoothing. Hysteresis (separate open/close thresholds) and a proper range
// (max attenuation) control are documented TODOs.
//
#include "IEffect.hpp"

namespace sonicpatch {

class NoiseGate final : public IEffect {
public:
    enum Param : uint32_t {
        kThresholdDb = 0,
        kAttackMs    = 1,
        kReleaseMs   = 2,
        kHoldMs      = 3,
        kRangeDb     = 4  ///< attenuation when closed (e.g. -80 dB). TODO: wire fully.
    };

    NoiseGate() = default;

    void prepare(const AudioFormat& fmt) override;
    void reset() override;
    void process(float** io, int channels, int frames) override; // [RT]
    void setParameter(uint32_t id, float value) override;
    const char* name() const override { return "NoiseGate"; }

private:
    void recompute();

    double sampleRate_ = 48000.0;
    float  thresholdDb_ = -50.0f;
    float  attackMs_    = 1.0f;
    float  releaseMs_   = 100.0f;
    float  holdMs_      = 10.0f;
    float  rangeDb_     = -80.0f;

    float  attackCoeff_  = 0.0f;
    float  releaseCoeff_ = 0.0f;
    int    holdSamples_  = 0;

    float  envGain_   = 0.0f; ///< 0 = closed, 1 = open
    int    holdCount_ = 0;
};

} // namespace sonicpatch
