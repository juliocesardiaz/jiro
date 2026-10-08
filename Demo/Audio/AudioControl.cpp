#include "AudioControl.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <new>

static_assert(std::atomic<float>::is_always_lock_free, "RT floats must be lock-free");
static_assert(std::atomic<uint32_t>::is_always_lock_free, "RT counters must be lock-free");

struct SPStrip {
    std::atomic<float> targetLeft{1}, targetRight{1}, peak{0};
    std::atomic<uint32_t> callbacks{0}, formatFault{0};
    float left = 0, right = 0, lastLeft = 0, lastRight = 0;
    float stepLeft = 0, stepRight = 0;
    uint32_t remaining = 0, rampFrames = 240, inputOffset = 0;
};

namespace {
struct Channel {
    float *data = nullptr;
    uint32_t stride = 0, frames = 0;
};
Channel channel(const SPBuffer *buffers, uint32_t count, uint32_t index) {
    if (!buffers) return {};
    for (uint32_t i = 0; i < count; ++i) {
        const auto &b = buffers[i];
        if (index < b.channels) {
            if (!b.data || !b.channels) return {};
            return {b.data + index, b.channels,
                    (b.byteSize / static_cast<uint32_t>(sizeof(float))) / b.channels};
        }
        index -= b.channels;
    }
    return {};
}
float finiteClamp(float value, float low, float high, float fallback) {
    return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
}
float sample(Channel c, uint32_t frame) {
    if (!c.data || frame >= c.frames) return 0;
    const float v = c.data[static_cast<size_t>(frame) * c.stride];
    return std::isfinite(v) ? v : 0;
}
}

SPStrip *SPStripCreate(double rate, uint32_t offset) {
    if (!std::isfinite(rate) || rate < 8000 || rate > 384000) return nullptr;
    auto *s = new (std::nothrow) SPStrip;
    if (s) {
        s->rampFrames = static_cast<uint32_t>(rate * 0.005); // 5 ms fade
        s->inputOffset = offset;
    }
    return s;
}
void SPStripDestroy(SPStrip *s) { delete s; }
void SPStripSetControls(SPStrip *s, float gain, float balance, bool muted) {
    if (!s) return;
    gain = muted ? 0 : finiteClamp(gain, 0, 1, 0);
    balance = finiteClamp(balance, -1, 1, 0);
    s->targetLeft.store(gain * (balance > 0 ? 1 - balance : 1), std::memory_order_relaxed);
    s->targetRight.store(gain * (balance < 0 ? 1 + balance : 1), std::memory_order_relaxed);
}
float SPStripTakePeak(SPStrip *s) {
    return s ? s->peak.exchange(0, std::memory_order_relaxed) : 0;
}
uint32_t SPStripCallbackCount(SPStrip *s) {
    return s ? s->callbacks.load(std::memory_order_relaxed) : 0;
}
bool SPStripHasFormatFault(SPStrip *s) {
    return s && s->formatFault.load(std::memory_order_relaxed);
}

void SPStripProcess(SPStrip *s, const SPBuffer *in, uint32_t inCount,
                    const SPBuffer *out, uint32_t outCount) {
    if (!out) return;
    for (uint32_t i = 0; i < outCount; ++i)
        if (out[i].data) std::memset(out[i].data, 0, out[i].byteSize);
    if (!s) return;
    s->callbacks.fetch_add(1, std::memory_order_relaxed);
    const auto il = channel(in, inCount, s->inputOffset);
    const auto ir = channel(in, inCount, s->inputOffset + 1);
    const auto ol = channel(out, outCount, 0);
    const auto oright = channel(out, outCount, 1);
    const uint32_t frames = std::max(ol.frames, oright.frames);
    const float tl = s->targetLeft.load(std::memory_order_relaxed);
    const float tr = s->targetRight.load(std::memory_order_relaxed);
    if (tl != s->lastLeft || tr != s->lastRight) {
        s->lastLeft = tl; s->lastRight = tr;
        s->remaining = s->rampFrames;
        s->stepLeft = (tl - s->left) / s->rampFrames;
        s->stepRight = (tr - s->right) / s->rampFrames;
    }
    float peak = 0;
    for (uint32_t f = 0; f < frames; ++f) {
        if (s->remaining) {
            s->left += s->stepLeft; s->right += s->stepRight;
            if (--s->remaining == 0) { s->left = tl; s->right = tr; }
        }
        const float l = sample(il, f) * s->left;
        const float r = sample(ir, f) * s->right;
        if (ol.data && f < ol.frames) ol.data[static_cast<size_t>(f) * ol.stride] = l;
        if (oright.data && f < oright.frames)
            oright.data[static_cast<size_t>(f) * oright.stride] = r;
        peak = std::max(peak, std::max(std::abs(l), std::abs(r)));
    }
    // One render writer; exchange on the UI side may skip a block peak, but
    // never races on raw memory and never spins on the real-time thread.
    s->peak.store(std::max(peak, s->peak.load(std::memory_order_relaxed)),
                  std::memory_order_relaxed);
}

#ifdef __APPLE__
namespace {
OSStatus render(AudioObjectID, const AudioTimeStamp *, const AudioBufferList *input,
                const AudioTimeStamp *, AudioBufferList *output,
                const AudioTimeStamp *, void *context) {
    constexpr uint32_t maxBuffers = 64;
    auto *s = static_cast<SPStrip *>(context);
    if (!output) return noErr;
    if (output->mNumberBuffers > maxBuffers ||
        (input && input->mNumberBuffers > maxBuffers)) {
        for (uint32_t i = 0; i < output->mNumberBuffers; ++i)
            if (output->mBuffers[i].mData)
                std::memset(output->mBuffers[i].mData, 0, output->mBuffers[i].mDataByteSize);
        s->formatFault.store(1, std::memory_order_relaxed);
        return noErr;
    }
    SPBuffer ins[maxBuffers]{}, outs[maxBuffers]{};
    const uint32_t ni = input ? input->mNumberBuffers : 0;
    for (uint32_t i = 0; i < ni; ++i) {
        const auto &b = input->mBuffers[i];
        ins[i] = {static_cast<float *>(b.mData), b.mNumberChannels, b.mDataByteSize};
    }
    for (uint32_t i = 0; i < output->mNumberBuffers; ++i) {
        const auto &b = output->mBuffers[i];
        outs[i] = {static_cast<float *>(b.mData), b.mNumberChannels, b.mDataByteSize};
    }
    SPStripProcess(s, ins, ni, outs, output->mNumberBuffers);
    return noErr;
}
}
OSStatus SPStripInstallIOProc(AudioObjectID device, SPStrip *strip,
                             AudioDeviceIOProcID *proc) {
    return AudioDeviceCreateIOProcID(device, render, strip, proc);
}
#endif
