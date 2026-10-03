//
// TapSourceNode.cpp
//
#include "TapSourceNode.hpp"

#include <algorithm>
#include <cstring>

#if defined(__APPLE__)
// TODO(Phase 1): #include <CoreAudio/CoreAudio.h> and the Tap headers
// (AudioHardwareTapping). The actual capture path uses:
//   * AudioHardwareCreateProcessTap()      — create a tap on the target process
//   * AudioHardwareCreateAggregateDevice() — aggregate that includes the tap
//   * AudioDeviceCreateIOProcID()/Start()  — run an IOProc that calls
//                                            depositFromTap() each cycle.
#endif

namespace sonicpatch {

TapSourceNode::TapSourceNode(NodeID id, const char* bundleId)
    : Node(NodeKind::TapSource, id), bundleId_(bundleId ? bundleId : "") {}

void TapSourceNode::prepare(const AudioFormat& fmt) {
    format_ = fmt;
    // Pre-allocate staging storage (non-RT).
    staging_.assign(fmt.channels, std::vector<float>(fmt.framesPerBuffer, 0.0f));
    stagedFrames_.store(0, std::memory_order_relaxed);
}

void TapSourceNode::reset() {
    for (auto& ch : staging_) std::fill(ch.begin(), ch.end(), 0.0f);
    stagedFrames_.store(0, std::memory_order_relaxed);
}

void TapSourceNode::depositFromTap(const float* const* src,
                                   uint32_t channels,
                                   uint32_t frames) noexcept {
    // [RT] Called from the Tap IOProc thread. Copies into pre-allocated staging.
    const uint32_t ch = std::min<uint32_t>(channels, static_cast<uint32_t>(staging_.size()));
    for (uint32_t c = 0; c < ch; ++c) {
        const uint32_t cap = static_cast<uint32_t>(staging_[c].size());
        const uint32_t n   = std::min(frames, cap);
        std::memcpy(staging_[c].data(), src[c], n * sizeof(float));
    }
    stagedFrames_.store(std::min(frames, format_.framesPerBuffer),
                        std::memory_order_release);
}

void TapSourceNode::process(ProcessContext& ctx) {
    // [RT] Drain staging into the output buffer; emit silence if nothing staged.
    if (ctx.numOutputs < 1 || !ctx.outputs[0].valid()) return;
    AudioBuffer& out = ctx.outputs[0];

    const uint32_t frames = ctx.frames;
    const uint32_t chans  = out.numChannels();
    const uint32_t avail  = stagedFrames_.load(std::memory_order_acquire);

    for (uint32_t c = 0; c < chans; ++c) {
        float* dst = out.channel(c);
        if (c < staging_.size() && avail > 0) {
            const uint32_t n = std::min(frames, avail);
            std::memcpy(dst, staging_[c].data(), n * sizeof(float));
            if (n < frames) {
                std::memset(dst + n, 0, (frames - n) * sizeof(float));
            }
        } else {
            std::memset(dst, 0, frames * sizeof(float)); // silence
        }
    }
}

} // namespace sonicpatch
