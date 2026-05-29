#pragma once
//
// EffectNode.hpp
// Wraps either a builtin DSP effect (IEffect*) or a hosted AudioUnit (Phase 4)
// and applies it in place to its input -> output.
//
// The node holds an owning unique_ptr<IEffect> for builtin effects. Hosted AUs
// are represented by an opaque AudioUnitHost handle (declared/owned in the AU
// layer, guarded by __APPLE__); for now that path is a documented stub.
//
#include "Node.hpp"
#include "../DSP/IEffect.hpp"

#include <memory>
#include <vector>

namespace sonicpatch {

class EffectNode final : public Node {
public:
    /// Takes ownership of a builtin effect instance.
    EffectNode(NodeID id, std::unique_ptr<IEffect> effect);

    void prepare(const AudioFormat& fmt) override;
    void reset() override;
    void process(ProcessContext& ctx) override; // [RT]

    /// Control thread: forward a parameter to the wrapped effect.
    void setParameter(uint32_t paramId, float value);

    IEffect* effect() noexcept { return effect_.get(); }

private:
    std::unique_ptr<IEffect> effect_;       ///< null if this wraps a hosted AU
    AudioFormat              format_;

    // Pre-allocated array of channel pointers handed to IEffect::process(); sized
    // in prepare() so process() never allocates.
    std::vector<float*>      channelPtrs_;

    // TODO(Phase 4): when wrapping a hosted AU, hold an AudioUnitHost* here
    // (guarded by __APPLE__) and route process() to it instead of effect_.
};

} // namespace sonicpatch
