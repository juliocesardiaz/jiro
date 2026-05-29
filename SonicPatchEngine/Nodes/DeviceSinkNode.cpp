//
// DeviceSinkNode.cpp
//
#include "DeviceSinkNode.hpp"

#include <algorithm>
#include <cstring>

#if defined(__APPLE__)
// TODO(Phase 1): #include <CoreAudio/CoreAudio.h>. The output path uses:
//   * AudioObjectGetPropertyData(kAudioHardwarePropertyDefaultOutputDevice)
//   * AudioDeviceCreateIOProcID() + AudioDeviceStart() to run an IOProc that
//     calls fetchForDevice() into the device's AudioBufferList each cycle.
#endif

namespace sonicpatch {

void DeviceSinkNode::prepare(const AudioFormat& fmt) {
    format_ = fmt;
    staging_.assign(fmt.channels, std::vector<float>(fmt.framesPerBuffer, 0.0f));
    readyFrames_.store(0, std::memory_order_relaxed);
}

void DeviceSinkNode::reset() {
    for (auto& ch : staging_) std::fill(ch.begin(), ch.end(), 0.0f);
    readyFrames_.store(0, std::memory_order_relaxed);
}

void DeviceSinkNode::process(ProcessContext& ctx) {
    // [RT] Copy the final mix into the staging area for the device IOProc.
    if (ctx.numInputs < 1 || !ctx.inputs[0].valid()) {
        readyFrames_.store(0, std::memory_order_release);
        return;
    }
    const AudioBuffer& in = ctx.inputs[0];

    const uint32_t frames = ctx.frames;
    const uint32_t chans  = std::min<uint32_t>(in.numChannels(),
                                               static_cast<uint32_t>(staging_.size()));
    for (uint32_t c = 0; c < chans; ++c) {
        const uint32_t cap = static_cast<uint32_t>(staging_[c].size());
        const uint32_t n   = std::min(frames, cap);
        std::memcpy(staging_[c].data(), in.channel(c), n * sizeof(float));
    }
    readyFrames_.store(std::min(frames, format_.framesPerBuffer),
                       std::memory_order_release);
}

void DeviceSinkNode::fetchForDevice(float* const* dst,
                                    uint32_t channels,
                                    uint32_t frames) noexcept {
    // [RT] Called from the device IOProc thread.
    const uint32_t avail = readyFrames_.load(std::memory_order_acquire);
    const uint32_t ch    = std::min<uint32_t>(channels,
                                              static_cast<uint32_t>(staging_.size()));
    for (uint32_t c = 0; c < ch; ++c) {
        const uint32_t n = std::min(frames, avail);
        std::memcpy(dst[c], staging_[c].data(), n * sizeof(float));
        if (n < frames) {
            std::memset(dst[c] + n, 0, (frames - n) * sizeof(float));
        }
    }
}

} // namespace sonicpatch
