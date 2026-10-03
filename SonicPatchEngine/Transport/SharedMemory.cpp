//
// SharedMemory.cpp
// POSIX shm_open + mmap implementation. Compiles on any POSIX platform; the
// macOS-specific fd/mach-port passing is left as a documented TODO.
//
#include "SharedMemory.hpp"

#include <utility>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>      // shm_open flags
#include <sys/mman.h>   // mmap, shm_open, shm_unlink
#include <sys/stat.h>   // mode constants
#include <unistd.h>     // ftruncate, close
#endif

namespace sonicpatch {

SharedMemory::~SharedMemory() { close(); }

SharedMemory::SharedMemory(SharedMemory&& other) noexcept {
    *this = std::move(other);
}

SharedMemory& SharedMemory::operator=(SharedMemory&& other) noexcept {
    if (this != &other) {
        close();
        name_  = std::move(other.name_);
        fd_    = other.fd_;
        base_  = other.base_;
        size_  = other.size_;
        owner_ = other.owner_;
        other.fd_    = -1;
        other.base_  = nullptr;
        other.size_  = 0;
        other.owner_ = false;
    }
    return *this;
}

bool SharedMemory::create(const std::string& name, size_t size) {
#if defined(__unix__) || defined(__APPLE__)
    close();
    name_  = name;
    owner_ = true;
    fd_ = ::shm_open(name.c_str(), O_CREAT | O_RDWR, 0600);
    if (fd_ < 0) return false;
    if (::ftruncate(fd_, static_cast<off_t>(size)) != 0) {
        close();
        return false;
    }
    void* p = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (p == MAP_FAILED) {
        close();
        return false;
    }
    base_ = p;
    size_ = size;
    return true;
#else
    (void)name; (void)size;
    return false; // shared memory only supported on POSIX targets
#endif
}

bool SharedMemory::open(const std::string& name, size_t size) {
#if defined(__unix__) || defined(__APPLE__)
    close();
    name_  = name;
    owner_ = false;
    fd_ = ::shm_open(name.c_str(), O_RDWR, 0600);
    if (fd_ < 0) return false;
    void* p = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (p == MAP_FAILED) {
        close();
        return false;
    }
    base_ = p;
    size_ = size;
    return true;
#else
    (void)name; (void)size;
    return false;
#endif
}

void SharedMemory::close() {
#if defined(__unix__) || defined(__APPLE__)
    if (base_ != nullptr && size_ > 0) {
        ::munmap(base_, size_);
    }
    if (fd_ >= 0) {
        ::close(fd_);
    }
    if (owner_ && !name_.empty()) {
        ::shm_unlink(name_.c_str());
    }
#endif
    base_  = nullptr;
    size_  = 0;
    fd_    = -1;
    owner_ = false;
    name_.clear();
}

} // namespace sonicpatch
