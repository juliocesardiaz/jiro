//
// MixerNode.cpp
//
#include "MixerNode.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace sonicpatch {

MixerNode::MixerNode(NodeID id, int numInputs) noexcept
    : Node(NodeKind::Mixer, id), numInputs_(std::max(0, numInputs)) {
    targets_ = std::make_unique<std::atomic<float>[]>(static_cast<size_t>(numInputs_));
    ramps_.resize(static_cast<size_t>(numInputs_));
    for (int i = 0; i < numInputs_; ++i) {
        targets_[static_cast<size_t>(i)].store(1.0f, std::memory_order_relaxed);
    }
}

void MixerNode::prepare(const AudioFormat&) {
    // No format-dependent allocation; gain state already sized to numInputs_.
}

void MixerNode::reset() {
    for (int i = 0; i < numInputs_; ++i) {
        Ramp& r      = ramps_[static_cast<size_t>(i)];
        r.current    = targets_[static_cast<size_t>(i)].load(std::memory_order_relaxed);
        r.rampTarget = r.current;
        r.step       = 0.0f;
    }
}

void MixerNode::setInputGain(int inputIndex, float linearGain) {
    if (inputIndex < 0 || inputIndex >= numInputs_) return;
    targets_[static_cast<size_t>(inputIndex)].store(linearGain, std::memory_order_relaxed);
}

void MixerNode::process(ProcessContext& ctx) {
    // [RT] Sum each input into the single output, ramping each input's gain
    // toward its target with a fixed per-sample slope that completes in
    // kRampSamples. Ramp state persists across blocks — with e.g. 32-frame
    // callbacks the ramp continues where the previous block left off instead
    // of snapping, so there is never a gain discontinuity at a block boundary.
    // No allocation, no locks.
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

        Ramp& ramp = ramps_[static_cast<size_t>(i)];
        const float target =
            targets_[static_cast<size_t>(i)].load(std::memory_order_relaxed);

        // New target since the last ramp was planned: compute a fresh
        // fixed-slope ramp from wherever the gain currently is.
        if (target != ramp.rampTarget) {
            ramp.rampTarget = target;
            ramp.step = (target - ramp.current) / static_cast<float>(kRampSamples);
        }

        const uint32_t mixChans = std::min(chans, in.numChannels());

        if (ramp.step == 0.0f && ramp.current == target) {
            // Settled: plain multiply-accumulate, channel-major for cache.
            const float gain = ramp.current;
            for (uint32_t c = 0; c < mixChans; ++c) {
                const float* src = in.channel(c);
                float*       dst = out.channel(c);
                for (uint32_t n = 0; n < frames; ++n) {
                    dst[n] += src[n] * gain;
                }
            }
            continue;
        }

        // Ramping: advance the gain once per frame and apply it to every
        // channel of that frame, so all channels share one trajectory and the
        // ramp lands exactly on the target (then holds).
        float    gain = ramp.current;
        float    step = ramp.step;
        for (uint32_t n = 0; n < frames; ++n) {
            if (step != 0.0f) {
                gain += step;
                const bool landed = (step > 0.0f) ? (gain >= target)
                                                  : (gain <= target);
                if (landed) {
                    gain = target;
                    step = 0.0f;
                }
            }
            for (uint32_t c = 0; c < mixChans; ++c) {
                out.channel(c)[n] += in.channel(c)[n] * gain;
            }
        }
        ramp.current = gain;
        ramp.step    = step;
    }
}

} // namespace sonicpatch
