#pragma once
//
// SharedMemory.hpp
// Thin POSIX shared-memory (shm_open + mmap) wrapper skeleton.
//
// The engine creates a named shared-memory region holding a RingBuffer (plus a
// small header). The HAL AudioServerPlugIn opens the same region by name and
// maps it read/write so the two processes share one lock-free ring. macOS also
// allows passing the mach port / fd via XPC; that path is noted as a TODO.
//
#include <cstddef>
#include <string>

namespace sonicpatch {

class SharedMemory {
public:
    SharedMemory() = default;
    ~SharedMemory();

    SharedMemory(const SharedMemory&)            = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;
    SharedMemory(SharedMemory&&) noexcept;
    SharedMemory& operator=(SharedMemory&&) noexcept;

    /// Create (and own) a region of `size` bytes named `name` (e.g.
    /// "/sonicpatch.ring"). Truncates and maps it RW. Non-RT. Returns true on
    /// success.
    bool create(const std::string& name, size_t size);

    /// Open an existing region by name and map it RW. Used by the HAL plugin.
    /// Non-RT. Returns true on success.
    bool open(const std::string& name, size_t size);

    /// Unmap and (if owner) unlink. Non-RT.
    void close();

    void*  data() const noexcept { return base_; }
    size_t size() const noexcept { return size_; }
    bool   valid() const noexcept { return base_ != nullptr; }

private:
    std::string name_;
    int         fd_    = -1;
    void*       base_  = nullptr;
    size_t      size_  = 0;
    bool        owner_ = false; ///< true if we created (and must unlink) it

    // TODO(Phase 5): expose the underlying fd so it can be sent to the driver
    // over XPC (sandboxed HAL plugins may not see the same shm namespace; fd or
    // mach-port passing is the robust route on macOS).
};

} // namespace sonicpatch
