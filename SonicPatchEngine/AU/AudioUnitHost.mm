//
// AudioUnitHost.mm
// ObjC++ implementation skeleton for hosting Audio Units (Phase 4).
//
// All real AudioToolbox/AVFoundation calls are guarded by __APPLE__ and shown in
// comments at the correct call sites. On non-Apple builds every method is a
// safe no-op so the engine links and the C++ unit tests run.
//
#include "AudioUnitHost.h"

#if defined(__APPLE__)
// TODO(Phase 4): real includes for hosting.
//   #import <AudioToolbox/AudioToolbox.h>
//   #import <AVFoundation/AVFoundation.h>   // AVAudioUnit, AVAudioUnitComponentManager
//   #import <CoreAudioKit/CoreAudioKit.h>   // for AUv3 view (if showing UI)
#endif

namespace sonicpatch {

AudioUnitHost::AudioUnitHost() = default;

AudioUnitHost::~AudioUnitHost() {
#if defined(__APPLE__)
    // TODO(Phase 4): if impl_ holds an AudioUnit:
    //   AudioUnitUninitialize((AudioUnit)impl_);
    //   AudioComponentInstanceDispose((AudioUnit)impl_);
    // or release the bridged AUAudioUnit*.
#endif
    impl_ = nullptr;
    instantiated_ = false;
}

int AudioUnitHost::scanComponents(uint32_t type, uint32_t subtype, uint32_t manufacturer) {
#if defined(__APPLE__)
    // TODO(Phase 4):
    //   AudioComponentDescription desc{ type, subtype, manufacturer, 0, 0 };
    //   AVAudioUnitComponentManager *mgr =
    //       [AVAudioUnitComponentManager sharedAudioUnitComponentManager];
    //   NSArray<AVAudioUnitComponent*> *comps =
    //       [mgr componentsMatchingDescription:desc];
    //   return (int)comps.count;
    (void)type; (void)subtype; (void)manufacturer;
    return 0;
#else
    (void)type; (void)subtype; (void)manufacturer;
    return 0;
#endif
}

bool AudioUnitHost::instantiate(const AudioComponentDescription& desc, const AudioFormat& fmt) {
#if defined(__APPLE__)
    // TODO(Phase 4): instantiate (AUv3 is async):
    //   [AVAudioUnit instantiateWithComponentDescription:desc
    //                                            options:kAudioComponentInstantiation_LoadOutOfProcess
    //                                  completionHandler:^(AVAudioUnit *au, NSError *err){ ... }];
    //   Configure maximumFramesToRender = fmt.framesPerBuffer, allocate render
    //   resources, set the AVAudioFormat to non-interleaved float32 @ fmt.sampleRate.
    (void)desc; (void)fmt;
    instantiated_ = false; // not yet implemented
    return false;
#else
    (void)desc; (void)fmt;
    return false;
#endif
}

void AudioUnitHost::render(float** io, int channels, int frames) noexcept {
#if defined(__APPLE__)
    // [RT] TODO(Phase 4): wrap io in an AudioBufferList and call the AU render
    // block:
    //   renderBlock(&flags, &timestamp, frames, 0, bufferList, pullInputBlock);
    // For now, pass-through (do nothing — io already holds the dry signal).
    (void)io; (void)channels; (void)frames;
#else
    (void)io; (void)channels; (void)frames;
#endif
}

void AudioUnitHost::setParameter(uint64_t address, float value) {
#if defined(__APPLE__)
    // TODO(Phase 4): auAudioUnit.parameterTree valueForKey... or
    //   AudioUnitSetParameter((AudioUnit)impl_, (AudioUnitParameterID)address,
    //                         kAudioUnitScope_Global, 0, value, 0);
    (void)address; (void)value;
#else
    (void)address; (void)value;
#endif
}

bool AudioUnitHost::saveState(void** outData, uint32_t* outLen) {
#if defined(__APPLE__)
    // TODO(Phase 4): read kAudioUnitProperty_ClassInfo (CFPropertyListRef) or
    // AUAudioUnit.fullState, serialize to a blob the caller frees.
    if (outData) *outData = nullptr;
    if (outLen)  *outLen  = 0;
    return false;
#else
    if (outData) *outData = nullptr;
    if (outLen)  *outLen  = 0;
    return false;
#endif
}

bool AudioUnitHost::restoreState(const void* data, uint32_t len) {
#if defined(__APPLE__)
    // TODO(Phase 4): set kAudioUnitProperty_ClassInfo / AUAudioUnit.fullState.
    (void)data; (void)len;
    return false;
#else
    (void)data; (void)len;
    return false;
#endif
}

} // namespace sonicpatch
