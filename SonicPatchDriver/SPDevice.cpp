//
// SPDevice.cpp
//
#include "SPDevice.hpp"

namespace sonicpatch {

bool SPDevice::onStartIO() {
    // Non-RT. Open the shared-memory ring and attach the reader.
    // TODO(Phase 5): open the engine's shared-memory region (by name or via an
    // fd received over XPC), then reader_.attach(mappedBase, capacity).
    ioRunning_ = true;
    return true;
}

void SPDevice::onStopIO() {
    // Non-RT. Detach + unmap.
    ioRunning_ = false;
    reader_.attach(nullptr, 0);
}

void SPDevice::readMix(float* const* dst, uint32_t channels, uint32_t frames) noexcept {
    // [RT] Pull processed frames from the ring (reader pads with silence if the
    // engine has not produced enough — underrun protection).
    reader_.read(dst, channels, frames);
}

} // namespace sonicpatch
