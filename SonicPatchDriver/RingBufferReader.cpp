//
// RingBufferReader.cpp
//
#include "RingBufferReader.hpp"

#include <cstring>

// Conceptually this reinterprets `base_` as a sonicpatch::RingBuffer<float, N>
// living in shared memory and drains it. The concrete capacity N must match the
// engine's writer; it is fixed by a shared header constant in a full build:
//
//   #include "../SonicPatchEngine/Transport/RingBuffer.hpp"
//   using SharedRing = sonicpatch::RingBuffer<float, kSharedRingCapacity>;
//
// We keep the skeleton free of that template instantiation so the driver target
// can compile independently; the read path is shown with the intended calls.

namespace sonicpatch {

bool RingBufferReader::attach(void* base, size_t frameCapacity) noexcept {
    base_     = base;
    capacity_ = frameCapacity;
    return base_ != nullptr;
}

uint32_t RingBufferReader::read(float* const* dst, uint32_t channels, uint32_t frames) noexcept {
    // [RT] No allocation, no locks.
    if (!base_) {
        // Not attached: output silence.
        for (uint32_t c = 0; c < channels; ++c) {
            std::memset(dst[c], 0, frames * sizeof(float));
        }
        return 0;
    }

    // TODO(Phase 5): interpret base_ as the shared RingBuffer and pull frames:
    //   auto* ring = static_cast<SharedRing*>(base_);
    //   For non-interleaved transport, either keep one ring per channel or read
    //   an interleaved block and de-interleave into dst[c]. Pad any shortfall
    //   with silence (below).
    for (uint32_t c = 0; c < channels; ++c) {
        std::memset(dst[c], 0, frames * sizeof(float));
    }
    return 0;
}

} // namespace sonicpatch
