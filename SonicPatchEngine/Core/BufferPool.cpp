//
// BufferPool.cpp
//
#include "BufferPool.hpp"

#include <cstdlib>
#include <cstring>
#include <new>

namespace sonicpatch {

namespace {

// Allocate `bytes` aligned to `alignment`. Uses aligned_alloc when available;
// alignment must divide the size. We round size up to a multiple of alignment.
float* allocAligned(size_t alignment, size_t frames) {
    const size_t bytes = frames * sizeof(float);
    const size_t rounded = ((bytes + alignment - 1) / alignment) * alignment;
#if defined(_ISOC11_SOURCE) || (defined(__cplusplus) && __cplusplus >= 201703L)
    void* p = std::aligned_alloc(alignment, rounded);
#else
    void* p = nullptr;
    if (posix_memalign(&p, alignment, rounded) != 0) p = nullptr;
#endif
    if (!p) return nullptr;
    std::memset(p, 0, rounded);
    return static_cast<float*>(p);
}

void freeAligned(float* p) {
    std::free(p); // aligned_alloc / posix_memalign are freed with free()
}

} // namespace

BufferPool::~BufferPool() { clear(); }

uint32_t BufferPool::sizeClassFor(uint32_t frames) {
    uint32_t sz = kMinFrames;
    while (sz < frames && sz < kMaxFrames) sz <<= 1;
    if (sz < kMinFrames) sz = kMinFrames;
    if (sz > kMaxFrames) sz = kMaxFrames;
    return sz;
}

bool BufferPool::reserve(uint32_t countPerSize) {
    clear();
    const uint32_t provisioned = countPerSize * 2; // 2x over-provision
    for (uint32_t sz = kMinFrames; sz <= kMaxFrames; sz <<= 1) {
        for (uint32_t i = 0; i < provisioned; ++i) {
            float* mem = allocAligned(kAlignment, sz);
            if (!mem) {
                clear(); // roll back
                return false;
            }
            Slot s;
            s.buf.data     = mem;
            s.buf.capacity = sz;
            s.inUse        = false;
            slots_.push_back(s);
        }
    }
    return true;
}

PooledBuffer* BufferPool::acquire(uint32_t frames, size_t& outIndex) {
    const uint32_t cls = sizeClassFor(frames);
    for (size_t i = 0; i < slots_.size(); ++i) {
        Slot& s = slots_[i];
        if (!s.inUse && s.buf.capacity == cls) {
            s.inUse  = true;
            outIndex = i;
            return &s.buf;
        }
    }
    outIndex = static_cast<size_t>(-1);
    return nullptr; // exhausted this size class
}

void BufferPool::release(size_t index) {
    if (index < slots_.size()) {
        slots_[index].inUse = false;
    }
}

void BufferPool::clear() {
    for (auto& s : slots_) {
        if (s.buf.data) freeAligned(s.buf.data);
    }
    slots_.clear();
}

} // namespace sonicpatch
