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
    // Peak decay ~ -20 dB over ~300 ms gives a readable peak meter.
    const double peakReleaseSec = 0.3;
    peakDecay_ = static_cast<float>(std::exp(-1.0 / (peakReleaseSec * sampleRate_)));
    // RMS integration window ~ 300 ms (one-pole on the squared signal).
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
    if (ctx.numInputs < 1 || !ctx.inputs[0].valid()) return;
    const AudioBuffer& in = ctx.inputs[0];

    const uint32_t frames = ctx.frames;
    const uint32_t chans  = in.numChannels();

    float blockPeak = 0.0f;
    for (uint32_t n = 0; n < frames; ++n) {
        for (uint32_t c = 0; c < chans; ++c) {
            const float s = in.channel(c)[n];
            blockPeak = std::max(blockPeak, std::fabs(s));
            // One-pole on the squared sample (mean-square estimate).
            rmsAccum_ = rmsCoeff_ * rmsAccum_ + (1.0f - rmsCoeff_) * (s * s);
        }
    }

    // Peak ballistics: jump up instantly, decay smoothly.
    peakHold_ *= peakDecay_;
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
