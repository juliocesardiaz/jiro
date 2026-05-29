#pragma once
//
// SPStream.hpp
// aspl::Stream subclass for the SonicPatch virtual device's output stream.
//
// Format: 32-bit float, non-interleaved, 48 kHz, 2 channels — matching the
// engine's internal format so no conversion is needed on the transport path.
//
// On non-Apple builds libASPL is unavailable, so the class is reduced to a
// portable descriptor holding the intended stream format. The Apple build
// derives from aspl::Stream and overrides the format-reporting hooks.
//
#include <cstdint>

#if defined(__APPLE__)
// TODO(Phase 5): #include <aspl/Stream.hpp>
#endif

namespace sonicpatch {

/// Portable description of the stream's PCM format (used on all platforms).
struct StreamFormat {
    double   sampleRate   = 48000.0;
    uint32_t channels     = 2;
    bool     isFloat      = true;
    uint32_t bitsPerChan  = 32;
    bool     interleaved  = false;
};

#if defined(__APPLE__)
// class SPStream : public aspl::Stream {
// public:
//     using aspl::Stream::Stream;
//     // TODO(Phase 5): override format hooks to advertise StreamFormat above via
//     //   AudioStreamBasicDescription { mSampleRate, kAudioFormatLinearPCM,
//     //     kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked |
//     //     kAudioFormatFlagIsNonInterleaved, ... }.
// };
#endif

} // namespace sonicpatch
