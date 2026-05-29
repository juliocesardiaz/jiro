//
// MixerNode.cpp
//
#include "MixerNode.hpp"

#include <algorithm>
#include <cstring>

namespace sonicpatch {

MixerNode::MixerNode(NodeID id, int numInputs) noexcept
    : Node(NodeKind::Mixer, id), numInputs_(numInputs) {
    gains_.resize(static_cast<size_t>(std::max(0, numInputs)));
}

void MixerNode::prepare(const AudioFormat&) {
    // No format-dependent allocation; gains_ already sized to numInputs_.
}

void MixerNode::reset() {
    for (auto& g : gains_) g.current = g.target;
}

void MixerNode::setInputGain(int inputIndex, float linearGain) {
    if (inputIndex < 0 || inputIndex >= numInputs_) return;
    gains_[static_cast<size_t>(inputIndex)].target = linearGain;
}

void MixerNode::process(ProcessContext& ctx) {
    // [RT] Sum each input into the single output with a 64-sample linear ramp
    // from current->target gain. No allocation.
    if (ctx.numOutputs < 1 || !ctx.outputs[0].valid()) return;

    AudioBuffer& out = ctx.outputs[0];
    const uint32_t frames = ctx.frames;
    const uint32_t chans  = out.numChannels();

    // Clear the output accumulator first.
    for (uint32_t c = 0; c < chans; ++c) {
        std::memset(out.channel(c), 0, frames * sizeof(float));
    }

    const int nIn = std::min(ctx.numInputs, numInputs_);
    for (int i = 0; i < nIn; ++i) {
        const AudioBuffer& in = ctx.inputs[i];
        if (!in.valid()) continue;

        InputGain& g = gains_[static_cast<size_t>(i)];

        // Per-input ramp increment: cover the difference over kRampSamples.
        const float start = g.current;
        const float end   = g.target;
        const float step  = (end - start) / static_cast<float>(kRampSamples);

        const uint32_t mixChans = std::min(chans, in.numChannels());
        for (uint32_t c = 0; c < mixChans; ++c) {
            const float* src = in.channel(c);
            float*       dst = out.channel(c);
            float gain = start;
            for (uint32_t n = 0; n < frames; ++n) {
                // Ramp for the first kRampSamples, then hold at target.
                if (n < static_cast<uint32_t>(kRampSamples)) {
                    gain = start + step * static_cast<float>(n + 1);
                } else {
                    gain = end;
                }
                dst[n] += src[n] * gain;
            }
        }
        // Advance the smoothed gain toward its target (one ramp per block).
        g.current = end;
    }
}

} // namespace sonicpatch
