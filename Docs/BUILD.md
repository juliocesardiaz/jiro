# Building SonicPatch

> **macOS only.** SonicPatch depends on Core Audio (Process Tap API, HAL),
> AudioToolbox, and Swift⇄C++ interop. **It does not build on Linux or in a
> non-macOS CI environment.** There is no cross-platform or container build path.

## Requirements

| Requirement | Version / Notes |
| ----------- | --------------- |
| macOS | 14.4 Sonoma or later (Process Tap API) |
| Architecture | Apple Silicon (arm64) |
| Xcode | 15.3 or later (Swift 5.9 toolchain) |
| Swift | 5.9+ (required for C++ interoperability) |
| XcodeGen | Latest (`brew install xcodegen`) |
| CMake | 3.20+ — only for the C++ engine GoogleTest suite |
| libASPL | Only for the HAL virtual-device target (Phase 5) |

## 1. Generate the Xcode project

`project.yml` is the **source of truth**; the `.xcodeproj` is generated and not
checked in.

```sh
brew install xcodegen
xcodegen generate          # produces SonicPatch.xcodeproj from project.yml
open SonicPatch.xcodeproj
```

Re-run `xcodegen generate` whenever `project.yml` or the folder layout changes.
Never edit the generated `.xcodeproj` by hand.

## 2. Build & run

Select the **SonicPatch** scheme and build/run (`Cmd-R`). The app launches as a
menu-bar agent (`LSUIElement`), so look for its icon in the menu bar rather than
the Dock. On first capture it will request audio-capture permission (TCC).

## 3. C++ / Objective-C++ interop build setting

The app calls the C++ engine (`sonicpatch::AudioEngine`) directly through Swift's
C++ interop. This is enabled per-target in `project.yml`:

```yaml
settings:
  base:
    SWIFT_OBJC_INTEROP_MODE: objcxx        # Xcode "C++ / Objective-C++" mode
    CLANG_CXX_LANGUAGE_STANDARD: "c++17"
    CLANG_CXX_LIBRARY: "libc++"
```

In Xcode this corresponds to **Build Settings → Swift Compiler – Language → C++
and Objective-C Interoperability = C++ / Objective-C++**. The engine's public
header (`SonicPatchEngine/Public/AudioEngine.hpp`) is dependency-free PODs so it
imports cleanly into Swift; the interop seam is isolated to
`SonicPatchApp/Services/EngineBridge.swift`.

## 4. Running the C++ engine unit tests (CMake + GoogleTest)

The C++ engine has its own test suite driven by **CMake/GoogleTest**, separate
from the Swift XCTest target. From the test directory:

```sh
cmake -S SonicPatchTests -B build/engine-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build/engine-tests
ctest --test-dir build/engine-tests --output-on-failure
```

(See `SonicPatchTests/CMakeLists.txt`, owned by the engine/test contributors.)
The Swift app layer is tested via the `SonicPatchTests` XCTest target inside
Xcode (`Cmd-U`).

## 5. libASPL (Phase 5 only)

The HAL virtual-device target (`SonicPatchDriver`) depends on
[libASPL](https://github.com/gavv/libASPL). It is vendored under `Vendor/` and is
**only** required to build the `.driver` bundle for inter-app routing. The core
capture/volume path (Phases 1–4) does not need it.

## Notes

- **Zero entitlements** for the core capture path — only the user's TCC audio-
  capture consent is required. The HAL driver install (Phase 5) is the sole
  privileged step and runs through a one-shot helper authorized with
  `AuthorizationRef`.
- Hardened Runtime is enabled (`ENABLE_HARDENED_RUNTIME`). Distribution builds
  must be signed and notarized (Phase 6).
