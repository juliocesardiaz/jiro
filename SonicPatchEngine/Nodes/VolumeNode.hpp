#pragma once
//
// VolumeNode.hpp
// dB gain + constant-power pan + mute, with per-sample one-pole smoothing to
// avoid zipper noise. Operates in place on its input -> output.
//
#include "Node.hpp"

#include <atomic>

namespace sonicpatch {

class VolumeNode final : public Node {
public:
    explicit VolumeNode(NodeID id) noexcept : Node(NodeKind::Volume, id) {}

    void prepare(const AudioFormat& fmt) override;
    void reset() override;
    void process(ProcessContext& ctx) override; // [RT]

    // Control thread setters (published via atomics; read on the audio thread).
    void setGainDb(float db) noexcept;
    void setPan(float pan) noexcept;      ///< -1..+1
    void setMuted(bool muted) noexcept;

    /// dB -> linear amplitude.
    static float dbToLinear(float db) noexcept;

private:
    double sampleRate_ = 48000.0;

    // Targets set from the control thread (atomic, lock-free reads on audio thread).
    std::atomic<float> targetGain_{1.0f}; ///< linear
    std::atomic<float> targetPan_{0.0f};
    std::atomic<bool>  muted_{false};

    // Smoothed instantaneous values (audio-thread-only state).
    float smoothedGain_ = 1.0f;
    float smoothedPan_  = 0.0f;
    float smoothCoeff_  = 0.0f; ///< one-pole coefficient derived in prepare()
};

} // namespace sonicpatch
