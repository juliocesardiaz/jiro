//
// PlugInMain.cpp
// AudioServerPlugIn entry point for the SonicPatch virtual audio driver.
//
// macOS loads a .driver bundle in coreaudiod and calls the factory function
// named in Info.plist (CFPlugInFactories) to obtain the plug-in interface. We
// use libASPL (https://github.com/gavv/libASPL) which wraps the
// AudioServerPlugIn C ABI in a modern C++ API; the factory returns an
// AudioServerPlugInDriverRef backed by an aspl::Driver.
//
// This file is a documented skeleton: the libASPL/CoreAudio calls are guarded by
// __APPLE__ so the repo builds on non-Apple CI for the pure-C++ portions.
//
#include <cstddef>

#if defined(__APPLE__)
// TODO(Phase 5): real includes.
//   #include <CoreAudio/AudioServerPlugIn.h>
//   #include <aspl/Driver.hpp>
//   #include <aspl/Context.hpp>
//   #include "SPDevice.hpp"
//   #include "SPStream.hpp"
#endif

extern "C" {

// The factory function referenced by Info.plist's CFPlugInFactories. coreaudiod
// calls this with the requested type UUID; we return our plug-in interface.
//
// Signature (real): void* SonicPatchPlugInFactory(CFAllocatorRef, CFUUIDRef);
// Declared as void* here to avoid pulling CoreFoundation into non-Apple builds.
void* SonicPatchPlugInFactory(void* allocator, const void* requestedTypeUUID) {
#if defined(__APPLE__)
    // TODO(Phase 5):
    //   static std::shared_ptr<aspl::Driver> driver = CreateDriver();
    //   return driver->GetReference();
    //
    // CreateDriver() builds an aspl::Context, an aspl::Plugin, one SPDevice with
    // one SPStream (32-bit float, non-interleaved, 48k), and returns the
    // aspl::Driver. The driver's IOProc reads from the shared-memory RingBuffer
    // produced by the engine (see RingBufferReader).
    (void)allocator; (void)requestedTypeUUID;
    return nullptr;
#else
    (void)allocator; (void)requestedTypeUUID;
    return nullptr; // HAL plug-ins only load on macOS
#endif
}

} // extern "C"
