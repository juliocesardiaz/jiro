#pragma once
//
// MixerNode.hpp
// Sums N inputs into one output with a per-input linear gain and a 64-sample
// linear de-zipper ramp to avoid clicks when a gain changes.
//
#include "Node.hpp"

#include <vector>

namespace sonicpatch {

class MixerNode final : public Node {
public:
    /// Length of the linear gain ramp applied when a target gain changes.
    static constexpr int kRampSamples = 64;

    MixerNode(NodeID id, int numInputs) noexcept;

    void prepare(const AudioFormat& fmt) override;
    void reset() override;
    void process(ProcessContext& ctx) override; // [RT]

    /// Control thread: set the target gain (linear) for one input.
    void setInputGain(int inputIndex, float linearGain);

private:
    struct InputGain {
        float current = 1.0f; ///< instantaneous (ramps toward target)
        float target  = 1.0f;
    };

    int                    numInputs_ = 0;
    std::vector<InputGain> gains_;     ///< sized in ctor/prepare; not in process
};

} // namespace sonicpatch
