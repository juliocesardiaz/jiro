#pragma once
//
// BufferPool.hpp
// Pre-allocated pool of page-aligned float buffers used for graph edges.
//
// Buffers come in power-of-two frame sizes (64..4096). The pool is over-
// provisioned 2x relative to the maximum simultaneous demand computed when the
// graph is built, so acquire() never has to allocate on a hot path. In practice
// acquire()/release() are performed at *setup/config-build* time (not on the
// audio thread): the GraphConfig records fixed buffer assignments and the audio
// thread merely reads from those pointers. The acquire/release API is therefore
// allowed to touch the free list, but is NOT called from process().
//
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sonicpatch {

/// A single channel's worth of aligned float storage owned by the pool.
struct PooledBuffer {
    float*   data     = nullptr; ///< page/SIMD-aligned storage.
    uint32_t capacity = 0;       ///< capacity in frames (a power of two).
};

class BufferPool {
public:
    /// Frame sizes the pool can hand out. All allocations are aligned to
    /// kAlignment for SIMD + cache friendliness.
    static constexpr uint32_t kMinFrames = 64;
    static constexpr uint32_t kMaxFrames = 4096;
    static constexpr size_t   kAlignment = 64; // bytes (cache line / AVX)

    BufferPool() = default;
    ~BufferPool();

    BufferPool(const BufferPool&)            = delete;
    BufferPool& operator=(const BufferPool&) = delete;

    /// Allocate all storage up front. For each power-of-two size class in
    /// [kMinFrames, kMaxFrames], allocate (2 * countPerSize) buffers (2x
    /// over-provisioning). Not RT-safe — call once at setup. Returns false if
    /// any allocation failed (and rolls everything back).
    bool reserve(uint32_t countPerSize);

    /// Acquire a free buffer that can hold at least `frames`. Rounds up to the
    /// nearest size class. Returns nullptr if none free. NOT for the audio
    /// thread (touches the free list). Index returned via `outIndex` for
    /// later release.
    PooledBuffer* acquire(uint32_t frames, size_t& outIndex);

    /// Return a previously-acquired buffer to the pool. NOT for the audio
    /// thread.
    void release(size_t index);

    /// Free everything. Not RT-safe.
    void clear();

    size_t totalBuffers() const { return slots_.size(); }

private:
    struct Slot {
        PooledBuffer buf;
        bool         inUse = false;
    };

    // Round `frames` up to the nearest power-of-two size class within range.
    static uint32_t sizeClassFor(uint32_t frames);

    std::vector<Slot> slots_; // all owned buffers, indexed by acquire handle.
};

} // namespace sonicpatch
