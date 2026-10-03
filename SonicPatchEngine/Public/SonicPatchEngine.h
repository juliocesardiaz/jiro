#pragma once
//
// SonicPatchEngine.h
// Umbrella header for the SonicPatch C++ audio engine framework.
//
// Importing this single header exposes the entire public API. It pulls in only
// the Swift-interop-safe public surface (no Core Audio / AudioToolbox includes),
// so it is safe to add to a module map for `import SonicPatchEngine` in Swift.
//
//   #include "SonicPatchEngine.h"
//   sonicpatch::AudioEngine engine;
//   engine.setAudioFormat({48000.0, 2, 512});
//   auto strip = engine.createChannelStrip("com.apple.Music");
//   engine.start();
//
#include "EngineTypes.hpp"
#include "AudioEngine.hpp"
