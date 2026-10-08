# SonicPatch

## Try the initial native demo

The `Demo/` target is a small native menu-bar mixer with live per-app volume,
mute, stereo balance, post-fader meters, output selection, and saved settings.
It uses a dedicated C++ processor for each tap; the larger graph/effects engine
below remains experimental. See **[Demo setup and playback checks](Docs/DEMO.md)**.

On an Apple Silicon Mac with Apple's developer tools installed:

```sh
bash scripts/build-demo.sh
```

The script builds, locally signs, and opens `build/SonicPatch Demo.app` without
Homebrew, XcodeGen, or a driver. Click **Start mixing** and allow system audio
capture. Use **Restore audio** to release every captured app.

**Verification:** portable audio regression tests are available through
`bash scripts/test-demo.sh`; the `Native demo` workflow builds on macOS.
A successful build does not replace the on-device playback checks in the guide.

---

## Full-engine architecture and roadmap

> A system-wide audio routing & processing engine for Apple Silicon Macs — per-app volume, inter-app routing, and a per-source channel strip with AU hosting and built-in DSP.

<!-- Badges placeholder -->
![status](https://img.shields.io/badge/status-pre--alpha-orange)
![platform](https://img.shields.io/badge/platform-macOS%2014.4%2B-blue)
![arch](https://img.shields.io/badge/arch-Apple%20Silicon%20arm64-lightgrey)
![license](https://img.shields.io/badge/license-MIT-green)

**Status: pre-alpha / Phase 1 in progress.** See the [Roadmap](Docs/ROADMAP.md).

SonicPatch captures audio from individual running applications using the macOS
Core Audio **Process Tap API** (introduced in macOS 14.4 Sonoma), runs each
source through a real-time C++ mixing/effects engine, and renders the result to
the device(s) you choose — all with a **zero-entitlements design** (no kernel
extensions, no privileged daemon required for the core capture path; the only
privileged step is the optional HAL virtual-device installer used for inter-app
routing in Phase 5).

## Features

- **Per-app volume & mute** — independent gain, mute, and pan for every
  audio-producing application, applied in real time without touching the app
  itself.
- **Inter-app routing** — route any source to any output device or virtual bus;
  send one app's audio into another app's input (Phase 5, via an optional
  user-space HAL virtual device built on [libASPL](https://github.com/gavv/libASPL)).
- **Per-source channel strip** — volume & pan with a peak/RMS meter per
  source, plus an ordered effect rack (Phase 3; input trim and a pre/post-FX
  split around the fader are planned).
- **AU hosting** — load and host Audio Unit (v2/v3) plugins in any insert slot
  (Phase 4).
- **Built-in DSP** — bundled real-time-safe effects (parametric EQ,
  compressor, limiter, noise gate, high/low-pass filter, gain utility) for when
  you don't want a third-party plugin.

## Requirements

| Requirement      | Version / Notes                                              |
| ---------------- | ------------------------------------------------------------ |
| macOS            | 14.4 Sonoma or later (Process Tap API requirement)           |
| Architecture     | Apple Silicon (arm64)                                         |
| Xcode            | 15.3 or later                                                |
| Swift            | 5.9+ (required for C++ interoperability with the engine)     |
| libASPL          | Only needed for the virtual-device / inter-app routing target |

## Architecture overview

SonicPatch is built in four layers. See [Docs/ARCHITECTURE.md](Docs/ARCHITECTURE.md)
for the canonical design.

1. **Capture layer — Core Audio Process Tap API.** Per-process taps feed audio
   into private aggregate devices with IOProc callbacks.
2. **Routing layer — HAL plugin (user-space virtual device).** An optional
   `.driver` bundle (libASPL-based) exposes virtual devices for inter-app
   routing.
3. **Engine layer — C++ real-time mixing/effects engine.** A lock-free audio
   graph of channel strips, gain/pan nodes, built-in DSP, and hosted Audio Units.
4. **App layer — SwiftUI menu-bar app.** UI, app monitoring, permission flow,
   session/preset management, and the C++ `EngineBridge`.

## Building

SonicPatch uses [XcodeGen](https://github.com/yonaskolb/XcodeGen) as the source
of truth for the Xcode project (`project.yml`). The generated `.xcodeproj` is
**not** checked in.

```sh
brew install xcodegen
xcodegen generate
open SonicPatch.xcodeproj   # or the generated workspace
```

> **Note:** SonicPatch only builds on macOS with Xcode. There is no Linux/CI
> build path for the app or the Core Audio layers. See [Docs/BUILD.md](Docs/BUILD.md)
> for the full setup, the C++ interop build setting, and how to run the C++
> engine unit tests via CMake.

## Project structure

```
.
├── project.yml                 # XcodeGen spec (source of truth)
├── LICENSE
├── README.md
├── CONTRIBUTING.md
├── Docs/
│   ├── ARCHITECTURE.md         # canonical design doc
│   ├── ROADMAP.md
│   └── BUILD.md
├── SonicPatchApp/              # SwiftUI menu-bar app (this layer)
│   ├── App/
│   ├── Views/
│   ├── Models/
│   ├── Services/
│   ├── TapManager/             # Process Tap API (Phase 1 centerpiece)
│   └── Info.plist
├── SonicPatchEngine/           # C++17 real-time engine (separate ownership)
├── SonicPatchDriver/           # HAL virtual-device .driver (separate ownership)
├── SonicPatchInstaller/        # privileged helper for HAL install (Phase 5)
├── SonicPatchTests/            # GoogleTest/CMake engine tests (no Swift tests yet)
└── Vendor/                     # third-party deps, e.g. libASPL
```

## Roadmap

See [Docs/ROADMAP.md](Docs/ROADMAP.md) for the six-phase plan and the current
status. We are in **Phase 1 (Tap proof-of-concept)** — code complete, pending
on-device verification.

## Contributing

Contributions are welcome — see [CONTRIBUTING.md](CONTRIBUTING.md) for branch/PR
conventions, code style, and dev setup.

## License

MIT — see [LICENSE](LICENSE). Copyright (c) 2026 SonicPatch contributors.
