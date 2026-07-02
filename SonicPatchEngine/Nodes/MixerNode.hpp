#pragma once
//
// MixerNode.hpp
// Sums N inputs into one output with a per-input linear gain and a 64-sample
// linear de-zipper ramp to avoid clicks when a gain changes.
//
#include "Node.hpp"

#include <atomic>
#include <memory>
#include <vector>

namespace sonicpatch {

class MixerNode final : public Node {
public:
    /// Nominal length of the linear gain ramp applied when a target changes.
    /// The ramp state persists across blocks: with callbacks shorter than this,
    /// the ramp simply continues in the next block (never snaps), converging
    /// geometrically onto the target.
    static constexpr int kRampSamples = 64;

    MixerNode(NodeID id, int numInputs) noexcept;

    void prepare(const AudioFormat& fmt) override;
    void reset() override;
    void process(ProcessContext& ctx) override; // [RT]

    /// Control thread: set the target gain (linear) for one input. Lock-free
    /// (atomic store); the audio thread ramps toward it.
    void setInputGain(int inputIndex, float linearGain);

private:
    int numInputs_ = 0;

    // Cross-thread: target gains written by the control thread, read by the
    // audio thread (atomics — std::atomic is not movable, hence the fixed
    // array rather than a vector).
    std::unique_ptr<std::atomic<float>[]> targets_;

    // Audio-thread-only ramp state. When the audio thread observes a target it
    // hasn't ramped to yet, it computes a fixed per-sample step sized to reach
    // it in kRampSamples; the ramp then advances sample-by-sample across block
    // boundaries until it lands exactly on the target.
    struct Ramp {
        float current    = 1.0f; ///< gain applied to the most recent sample
        float step       = 0.0f; ///< per-sample increment (0 when settled)
        float rampTarget = 1.0f; ///< target the current step was computed for
    };
    std::vector<Ramp> ramps_;
};

} // namespace sonicpatch
