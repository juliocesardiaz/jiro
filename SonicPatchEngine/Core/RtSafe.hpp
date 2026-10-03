#pragma once
//
// RtSafe.hpp
// Real-time-safety primitives and the rules that govern audio-thread code.
//
// ============================ RT RULES =====================================
// Audio-thread code (anything reachable from AudioGraph::process / a Node's
// process()) MUST NOT:
//   * allocate or free memory (no new/delete/malloc/free, no growing STL ctrs)
//   * take a lock (no std::mutex, no @synchronized, no os_unfair_lock blocking)
//   * make a syscall that can block (no I/O, no logging to disk, no printf)
//   * throw exceptions
// Everything the audio thread touches must be pre-allocated during setup() and
// communicated via atomics / lock-free queues. Functions that run on the audio
// thread are tagged with a `// [RT]` comment in this codebase.
// ===========================================================================
//
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace sonicpatch {

/// A single atomic float for publishing a continuously-updated value (e.g. a
/// meter level) from the audio thread to the UI thread. Relaxed ordering is
/// fine: meters tolerate the occasional stale read and there is no dependent
/// data that must be ordered with it.
class RealtimeLevel {
public:
    RealtimeLevel() noexcept : value_(0.0f) {}
    explicit RealtimeLevel(float v) noexcept : value_(v) {}

    // [RT] called from audio thread.
    void store(float v) noexcept { value_.store(v, std::memory_order_relaxed); }

    // Called from UI thread.
    float load() const noexcept { return value_.load(std::memory_order_relaxed); }

private:
    std::atomic<float> value_;
    static_assert(std::atomic<float>::is_always_lock_free,
                  "std::atomic<float> must be lock-free on the target platform");
};

/// Single-producer / single-consumer lock-free queue of trivially-copyable
/// values. Capacity is rounded up to a power of two so head/tail wrap with a
/// mask instead of a modulo. Used to ship parameter changes and commands from
/// the UI thread (producer) to the audio thread (consumer), or metering events
/// the other way. Storage is fixed at construction — no allocation after that.
///
/// One slot is always kept empty so a full ring is distinguishable from empty.
template <typename T, size_t Capacity>
class SpscQueue {
    static_assert(std::is_trivially_copyable<T>::value,
                  "SpscQueue<T> requires a trivially-copyable T (RT safety)");
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");

public:
    SpscQueue() noexcept : head_(0), tail_(0) {}

    /// Producer side. Returns false if the queue is full. [RT] when the
    /// producer is the audio thread; otherwise UI-thread. Never blocks.
    bool push(const T& item) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next = (head + 1) & kMask;
        if (next == tail_.load(std::memory_order_acquire)) {
            return false; // full
        }
        buffer_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    /// Consumer side. Returns false if empty. [RT] when the consumer is the
    /// audio thread. Never blocks.
    bool pop(T& out) noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false; // empty
        }
        out = buffer_[tail];
        tail_.store((tail + 1) & kMask, std::memory_order_release);
        return true;
    }

    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

private:
    static constexpr size_t kMask = Capacity - 1;

    // Cache-line padding to avoid false sharing between producer & consumer.
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
    alignas(64) T                   buffer_[Capacity];
};

} // namespace sonicpatch
