//
// EffectNode.cpp
//
#include "EffectNode.hpp"

#include <algorithm>
#include <cstring>

namespace sonicpatch {

EffectNode::EffectNode(NodeID id, std::unique_ptr<IEffect> effect)
    : Node(NodeKind::Effect, id), effect_(std::move(effect)) {}

void EffectNode::prepare(const AudioFormat& fmt) {
    format_ = fmt;
    if (effect_) effect_->prepare(fmt);
    // Pre-size the channel-pointer scratch so process() allocates nothing.
    channelPtrs_.assign(fmt.channels, nullptr);
}

void EffectNode::reset() {
    if (effect_) effect_->reset();
}

void EffectNode::setParameter(uint32_t paramId, float value) {
    if (effect_) effect_->setParameter(paramId, value);
}

void EffectNode::process(ProcessContext& ctx) {
    // [RT] In-place: copy input->output (if distinct), then process in place.
    if (ctx.numInputs < 1 || ctx.numOutputs < 1) return;
    const AudioBuffer& in  = ctx.inputs[0];
    AudioBuffer&       out = ctx.outputs[0];
    if (!in.valid() || !out.valid()) return;

    const uint32_t frames = ctx.frames;
    const uint32_t chans  = std::min(in.numChannels(), out.numChannels());

    // Copy input into output when they are not the same storage.
    if (out.channel(0) != in.channel(0)) {
        for (uint32_t c = 0; c < chans; ++c) {
            std::memcpy(out.channel(c), in.channel(c), frames * sizeof(float));
        }
    }

    if (!effect_) {
        // TODO(Phase 4): route to the hosted AudioUnit render here.
        return;
    }

    // Gather output channel pointers (pre-allocated vector, no alloc).
    const uint32_t n = std::min<uint32_t>(chans, static_cast<uint32_t>(channelPtrs_.size()));
    for (uint32_t c = 0; c < n; ++c) {
        channelPtrs_[c] = out.channel(c);
    }
    effect_->process(channelPtrs_.data(), static_cast<int>(n), static_cast<int>(frames));
}

} // namespace sonicpatch
