//
// SplitterNode.cpp
//
#include "SplitterNode.hpp"

#include <algorithm>
#include <cstring>

namespace sonicpatch {

void SplitterNode::process(ProcessContext& ctx) {
    // [RT] Copy input[0] to every output. No allocation.
    if (ctx.numInputs < 1 || !ctx.inputs[0].valid()) return;

    const AudioBuffer& in = ctx.inputs[0];
    const uint32_t frames = ctx.frames;

    for (int o = 0; o < ctx.numOutputs; ++o) {
        AudioBuffer& out = ctx.outputs[o];
        if (!out.valid()) continue;

        const uint32_t chans = std::min(in.numChannels(), out.numChannels());
        for (uint32_t c = 0; c < chans; ++c) {
            std::memcpy(out.channel(c), in.channel(c), frames * sizeof(float));
        }
        // Zero any extra output channels the input doesn't supply.
        for (uint32_t c = chans; c < out.numChannels(); ++c) {
            std::memset(out.channel(c), 0, frames * sizeof(float));
        }
    }
}

} // namespace sonicpatch
