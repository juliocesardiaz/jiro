#pragma once
//
// RingBufferReader.hpp
// Reads processed audio out of the shared-memory ring produced by the engine.
//
// The engine maps a shared-memory region (see Engine/Transport/SharedMemory) and
// places a RingBuffer<float, N> in it. The HAL plug-in opens the same region and
// uses this reader to pull interleaved-by-channel float frames into the device's
// IOProc output buffers. read() is called on coreaudiod's RT thread, so it must
// be lock-free and allocation-free.
//
#include <cstddef>
#include <cstdint>

namespace sonicpatch {

class RingBufferReader {
public:
    RingBufferReader() = default;

    /// Attach to a shared-memory region already opened by the caller. `base`
    /// points at the mapped RingBuffer; `frameCapacity` documents its size.
    /// Non-RT (setup). Returns false if base is null.
    bool attach(void* base, size_t frameCapacity) noexcept;

    /// [RT] Pull up to `frames` per channel into `dst` (array of `channels`
    /// pointers). Returns frames actually read; pads the remainder with silence.
    uint32_t read(float* const* dst, uint32_t channels, uint32_t frames) noexcept;

    bool valid() const noexcept { return base_ != nullptr; }

private:
    void*  base_     = nullptr; ///< mapped RingBuffer (not owned)
    size_t capacity_ = 0;
};

} // namespace sonicpatch
