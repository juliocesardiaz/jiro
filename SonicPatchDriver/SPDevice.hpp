#pragma once
//
// SPDevice.hpp
// aspl::Device subclass for the SonicPatch virtual output device.
//
// Responsibilities:
//   * Own one output SPStream (32-bit float / 48k / non-interleaved).
//   * On OnStartIO(): open the shared-memory ring and prepare the reader.
//   * In the IO request (ReadClientInput / WriteMix path): pull processed audio
//     from the engine's ring via RingBufferReader into the device's buffers.
//   * On OnStopIO(): release the ring mapping.
//
// On non-Apple builds the libASPL base is unavailable, so this declares the
// portable members and the intended override surface as comments.
//
#include "RingBufferReader.hpp"
#include "SPStream.hpp"

#if defined(__APPLE__)
// TODO(Phase 5): #include <aspl/Device.hpp>
#endif

namespace sonicpatch {

class SPDevice {
public:
    SPDevice() = default;

    /// Open the shared-memory transport and ready the reader. Non-RT.
    /// Mirrors aspl::Device::OnStartIO. Returns true on success.
    bool onStartIO();

    /// Release the transport. Non-RT. Mirrors aspl::Device::OnStopIO.
    void onStopIO();

    /// [RT] Fill `dst` (channels x frames) with processed audio from the ring.
    /// Invoked from the device IO handler on coreaudiod's RT thread.
    void readMix(float* const* dst, uint32_t channels, uint32_t frames) noexcept;

private:
    StreamFormat      format_;
    RingBufferReader  reader_;
    bool              ioRunning_ = false;

    // TODO(Phase 5): hold the shared-memory mapping (or its fd received via XPC)
    // and the aspl::Device/Stream shared_ptrs on Apple builds.
};

} // namespace sonicpatch
