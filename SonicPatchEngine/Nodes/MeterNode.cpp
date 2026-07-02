//
// MeterNode.cpp
//
#include "MeterNode.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace sonicpatch {

void MeterNode::prepare(const AudioFormat& fmt) {
    sampleRate_ = fmt.sampleRate;
    // Peak release time constant ~300 ms. peakDecay_ is the PER-SAMPLE
    // coefficient; process() raises it to the block length so ballistics are
    // independent of callback size.
    const double peakReleaseSec = 0.3;
    peakDecay_ = static_cast<float>(std::exp(-1.0 / (peakReleaseSec * sampleRate_)));
    // RMS integration window ~ 300 ms (one-pole on the squared signal, stepped
    // once per FRAME on the mean square across channels).
    const double rmsWindowSec = 0.3;
    rmsCoeff_ = static_cast<float>(std::exp(-1.0 / (rmsWindowSec * sampleRate_)));
    reset();
}

void MeterNode::reset() {
    peakHold_ = 0.0f;
    rmsAccum_ = 0.0f;
    peak_.store(0.0f);
    rms_.store(0.0f);
}

void MeterNode::process(ProcessContext& ctx) {
    // [RT] Pass-through (copy in->out if distinct), measure peak/RMS, publish.
    // Per-block decay is peakDecay_^frames so the release time is a real time
    // constant regardless of callback size (std::pow is deterministic and
    // allocation-free — acceptable once per block).
    if (ctx.numInputs < 1 || !ctx.inputs[0].valid()) {
        // No signal: keep decaying instead of freezing at the last value.
        const uint32_t frames = ctx.frames > 0 ? ctx.frames : 1;
        const float blockDecay = std::pow(peakDecay_, static_cast<float>(frames));
        peakHold_ *= blockDecay;
        rmsAccum_ *= blockDecay;
        peak_.store(peakHold_);
        rms_.store(std::sqrt(rmsAccum_));
        return;
    }
    const AudioBuffer& in = ctx.inputs[0];

    const uint32_t frames = ctx.frames;
    const uint32_t chans  = in.numChannels();
    const float invChans  = chans > 0 ? 1.0f / static_cast<float>(chans) : 0.0f;

    float blockPeak = 0.0f;
    for (uint32_t n = 0; n < frames; ++n) {
        float frameSq = 0.0f;
        for (uint32_t c = 0; c < chans; ++c) {
            const float s = in.channel(c)[n];
            blockPeak = std::max(blockPeak, std::fabs(s));
            frameSq += s * s;
        }
        // One-pole on the per-frame mean square (channel-averaged) so the
        // window is 300 ms regardless of channel count.
        rmsAccum_ = rmsCoeff_ * rmsAccum_ + (1.0f - rmsCoeff_) * (frameSq * invChans);
    }

    // Peak ballistics: jump up instantly, decay smoothly (per-sample coefficient
    // raised to the block length).
    peakHold_ *= std::pow(peakDecay_, static_cast<float>(frames));
    if (blockPeak > peakHold_) peakHold_ = blockPeak;

    peak_.store(peakHold_);
    rms_.store(std::sqrt(rmsAccum_));

    // Pass-through: if the output buffer is distinct from the input, copy.
    if (ctx.numOutputs >= 1 && ctx.outputs[0].valid()) {
        AudioBuffer& out = ctx.outputs[0];
        if (out.numChannels() > 0 && out.channel(0) != in.channel(0)) {
            const uint32_t outChans = std::min(chans, out.numChannels());
            for (uint32_t c = 0; c < outChans; ++c) {
                std::memcpy(out.channel(c), in.channel(c), frames * sizeof(float));
            }
        }
    }
}

} // namespace sonicpatch
