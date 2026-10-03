#pragma once
//
// AudioBuffer.hpp
// Non-owning, non-interleaved float32 buffer *view*.
//
// An AudioBuffer is a lightweight handle: an array of per-channel pointers plus
// a frame count. It owns nothing — the storage is provided by the BufferPool
// (for intermediate graph edges) or by Core Audio (for I/O endpoints). This is
// trivially copyable and safe to pass around on the audio thread (no alloc).
//
#include <cstdint>
#include <cassert>

namespace sonicpatch {

class AudioBuffer {
public:
    AudioBuffer() = default;

    /// @param channelPtrs array of `numChannels` pointers, one per channel.
    /// @param numChannels number of channels.
    /// @param numFrames   valid frames in each channel buffer.
    AudioBuffer(float* const* channelPtrs, uint32_t numChannels, uint32_t numFrames) noexcept
        : channels_(channelPtrs), numChannels_(numChannels), numFrames_(numFrames) {}

    // --- Audio-thread-safe accessors (no allocation, no locks) -------------
    uint32_t numChannels() const noexcept { return numChannels_; }
    uint32_t numFrames()   const noexcept { return numFrames_; }
    bool     valid()       const noexcept { return channels_ != nullptr && numChannels_ > 0; }

    /// Mutable pointer to one channel's samples.
    float* channel(uint32_t ch) noexcept {
        assert(ch < numChannels_);
        return channels_[ch];
    }
    /// Const pointer to one channel's samples.
    const float* channel(uint32_t ch) const noexcept {
        assert(ch < numChannels_);
        return channels_[ch];
    }

    /// Reinterpret with fewer frames (e.g. a partial last block). Cheap copy.
    AudioBuffer withFrames(uint32_t frames) const noexcept {
        return AudioBuffer(channels_, numChannels_, frames);
    }

private:
    float* const* channels_   = nullptr;  ///< Array of channel pointers (not owned).
    uint32_t      numChannels_ = 0;
    uint32_t      numFrames_   = 0;
};

} // namespace sonicpatch
