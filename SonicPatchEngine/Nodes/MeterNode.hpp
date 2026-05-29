#pragma once
//
// MeterNode.hpp
// Pass-through metering tap. Computes peak (with decay ballistics) and RMS
// (sliding one-pole) and publishes them to atomics for lock-free UI reads.
//
#include "Node.hpp"
#include "../Core/RtSafe.hpp"

namespace sonicpatch {

class MeterNode final : public Node {
public:
    explicit MeterNode(NodeID id) noexcept : Node(NodeKind::Meter, id) {}

    void prepare(const AudioFormat& fmt) override;
    void reset() override;
    void process(ProcessContext& ctx) override; // [RT]

    /// Lock-free read for the UI thread. Linear (not dB) values.
    LevelSnapshot snapshot() const noexcept {
        LevelSnapshot s;
        s.peak = peak_.load();
        s.rms  = rms_.load();
        return s;
    }

private:
    double sampleRate_ = 48000.0;

    // Published levels (audio thread writes via RealtimeLevel::store).
    RealtimeLevel peak_;
    RealtimeLevel rms_;

    // Audio-thread-only ballistics state.
    float peakHold_      = 0.0f;
    float rmsAccum_      = 0.0f;
    float peakDecay_     = 0.0f; ///< per-block multiplicative decay
    float rmsCoeff_      = 0.0f; ///< one-pole smoothing for mean-square
};

} // namespace sonicpatch
