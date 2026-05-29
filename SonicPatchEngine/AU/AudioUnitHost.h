#pragma once
//
// AudioUnitHost.h
// ObjC++ interface for hosting third-party Audio Units (Phase 4).
//
// This declares a thin C++ wrapper around an AUv2/AUv3 instance so the rest of
// the engine (which is plain C++) can host effects without importing Objective-C
// or AudioToolbox. The implementation lives in AudioUnitHost.mm and is gated on
// __APPLE__; on other platforms the methods are no-op stubs that report failure.
//
// Keep this header free of AudioToolbox includes so it can be referenced from
// portable C++ TUs; the AudioComponentDescription is forward-declared.
//
#include "../Public/EngineTypes.hpp"

#include <cstdint>

#if defined(__APPLE__)
struct AudioComponentDescription; // <AudioToolbox/AUComponent.h>
#else
struct AudioComponentDescription;  // opaque tag on non-Apple builds
#endif

namespace sonicpatch {

/// Owns and drives a single hosted AudioUnit. All methods are non-RT except
/// render(), which is the [RT] audio callback.
class AudioUnitHost {
public:
    AudioUnitHost();
    ~AudioUnitHost();

    AudioUnitHost(const AudioUnitHost&)            = delete;
    AudioUnitHost& operator=(const AudioUnitHost&) = delete;

    /// Enumerate available components matching `type`/`subtype`/`manufacturer`
    /// (0 = wildcard). Returns the count discovered; details retrieved via the
    /// platform AU APIs. Non-RT. (Stub: returns 0 off-Apple.)
    int scanComponents(uint32_t type, uint32_t subtype, uint32_t manufacturer);

    /// Instantiate the AU described by `desc` and prepare it for `fmt`.
    /// Non-RT, may block (AUv3 instantiation is async on real macOS). Returns
    /// true on success.
    bool instantiate(const AudioComponentDescription& desc, const AudioFormat& fmt);

    /// [RT] Render `frames` of `channels` in place through the hosted AU.
    void render(float** io, int channels, int frames) noexcept;

    /// Set an AU parameter by address (non-RT or RT-safe scheduling).
    void setParameter(uint64_t address, float value);

    /// Serialize / restore full AU state (presets) as an opaque blob. Non-RT.
    bool saveState(void** outData, uint32_t* outLen);
    bool restoreState(const void* data, uint32_t len);

    bool isInstantiated() const noexcept { return instantiated_; }

private:
    bool instantiated_ = false;

    // Opaque platform handle (an AudioUnit / AUAudioUnit*). Defined only in the
    // .mm; kept as void* here to avoid leaking AudioToolbox into C++ TUs.
    void* impl_ = nullptr;
};

} // namespace sonicpatch
