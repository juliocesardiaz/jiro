#pragma once
//
// RingBuffer.hpp
// Header-only single-producer / single-consumer lock-free byte/sample ring.
//
// Used to transfer processed audio from the engine (producer) to the HAL
// AudioServerPlugIn (consumer) across a shared-memory mapping. Capacity is a
// power of two so the head/tail indices wrap with a bitmask. Element type T must
// be trivially copyable. Both ends use only atomic loads/stores — no locks, no
// allocation in read()/write(), so both are RT-safe.
//
// The control block (head/tail) and the data array can live in shared memory:
// place a RingBuffer in an mmap'd region and both processes operate on it. The
// atomics must be lock-free for cross-process use (verified via static_assert).
//
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace sonicpatch {

template <typename T, size_t Capacity>
class RingBuffer {
    static_assert(std::is_trivially_copyable<T>::value,
                  "RingBuffer<T> requires trivially-copyable T");
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two >= 2");

public:
    RingBuffer() noexcept : head_(0), tail_(0) {}

    /// [RT] Producer: write up to `count` elements. Returns the number actually
    /// written (may be fewer if the buffer is near-full). Never blocks.
    size_t write(const T* src, size_t count) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_acquire);
        const size_t freeSpace = capacityMask() - ((head - tail) & capacityMask());
        const size_t n = count < freeSpace ? count : freeSpace;

        for (size_t i = 0; i < n; ++i) {
            data_[(head + i) & capacityMask()] = src[i];
        }
        head_.store(head + n, std::memory_order_release);
        return n;
    }

    /// [RT] Consumer: read up to `count` elements. Returns the number actually
    /// read. Never blocks.
    size_t read(T* dst, size_t count) noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t available = (head - tail) & capacityMask();
        const size_t n = count < available ? count : available;

        for (size_t i = 0; i < n; ++i) {
            dst[i] = data_[(tail + i) & capacityMask()];
        }
        tail_.store(tail + n, std::memory_order_release);
        return n;
    }

    /// Number of elements available to read (snapshot; may grow concurrently).
    size_t readAvailable() const noexcept {
        return (head_.load(std::memory_order_acquire) -
                tail_.load(std::memory_order_acquire)) & capacityMask();
    }

    /// Free element slots available for writing.
    size_t writeAvailable() const noexcept {
        return capacityMask() - readAvailable();
    }

    void clear() noexcept {
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

    static constexpr size_t capacity() noexcept { return Capacity; }

private:
    // We keep one slot unused so head==tail unambiguously means empty; the usable
    // capacity is therefore Capacity-1, expressed via the mask arithmetic above.
    static constexpr size_t capacityMask() noexcept { return Capacity - 1; }

    static_assert(std::atomic<size_t>::is_always_lock_free,
                  "ring indices must be lock-free for cross-process shared memory");

    alignas(64) std::atomic<size_t> head_; ///< written by producer
    alignas(64) std::atomic<size_t> tail_; ///< written by consumer
    alignas(64) T                   data_[Capacity];
};

} // namespace sonicpatch
