//
// VolumeNode.cpp
//
#include "VolumeNode.hpp"

#include <algorithm>
#include <cmath>

namespace sonicpatch {

namespace {
constexpr float kPi = 3.14159265358979323846f;
} // namespace

float VolumeNode::dbToLinear(float db) noexcept {
    return std::pow(10.0f, db * 0.05f);
}

void VolumeNode::prepare(const AudioFormat& fmt) {
    sampleRate_ = fmt.sampleRate;
    // ~5 ms smoothing time constant: coeff = exp(-1 / (tau * fs)).
    const double tau = 0.005;
    smoothCoeff_ = static_cast<float>(std::exp(-1.0 / (tau * sampleRate_)));
    reset();
}

void VolumeNode::reset() {
    smoothedGain_ = targetGain_.load(std::memory_order_relaxed);
    smoothedPan_  = targetPan_.load(std::memory_order_relaxed);
}

void VolumeNode::setGainDb(float db) noexcept {
    targetGain_.store(dbToLinear(db), std::memory_order_relaxed);
}

void VolumeNode::setPan(float pan) noexcept {
    targetPan_.store(std::clamp(pan, -1.0f, 1.0f), std::memory_order_relaxed);
}

void VolumeNode::setMuted(bool muted) noexcept {
    muted_.store(muted, std::memory_order_relaxed);
}

void VolumeNode::process(ProcessContext& ctx) {
    // [RT] in-place gain + constant-power pan + mute with one-pole smoothing.
    if (ctx.numInputs < 1 || ctx.numOutputs < 1) return;
    const AudioBuffer& in  = ctx.inputs[0];
    AudioBuffer&       out = ctx.outputs[0];
    if (!in.valid() || !out.valid()) return;

    const uint32_t frames = ctx.frames;
    const uint32_t chans  = std::min(in.numChannels(), out.numChannels());

    const float targetGain = muted_.load(std::memory_order_relaxed)
                                 ? 0.0f
                                 : targetGain_.load(std::memory_order_relaxed);
    const float targetPan  = targetPan_.load(std::memory_order_relaxed);
    const float c          = smoothCoeff_;

    if (chans == 2) {
        for (uint32_t n = 0; n < frames; ++n) {
            smoothedGain_ = c * smoothedGain_ + (1.0f - c) * targetGain;
            smoothedPan_  = c * smoothedPan_  + (1.0f - c) * targetPan;

            // Constant-power pan law: theta in [0, pi/2].
            const float theta = (smoothedPan_ * 0.5f + 0.5f) * (kPi * 0.5f);
            const float lGain = std::cos(theta) * smoothedGain_;
            const float rGain = std::sin(theta) * smoothedGain_;

            out.channel(0)[n] = in.channel(0)[n] * lGain;
            out.channel(1)[n] = in.channel(1)[n] * rGain;
        }
    } else {
        for (uint32_t n = 0; n < frames; ++n) {
            smoothedGain_ = c * smoothedGain_ + (1.0f - c) * targetGain;
            for (uint32_t ch = 0; ch < chans; ++ch) {
                out.channel(ch)[n] = in.channel(ch)[n] * smoothedGain_;
            }
        }
    }
}

} // namespace sonicpatch
