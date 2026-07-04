# Contributing to SonicPatch

Thanks for your interest in SonicPatch! This document covers how to set up a dev
environment, our branch/PR conventions, and the code style rules for both the
Swift app and the C++ engine.

SonicPatch is **pre-alpha** (Phase 1). Expect churn. If you're planning a large
change, please open an issue first so we can align on design — the canonical
design lives in [Docs/ARCHITECTURE.md](Docs/ARCHITECTURE.md).

## Dev setup

SonicPatch only builds on **macOS 14.4+** with **Xcode 15.3+** on **Apple
Silicon**. There is no Linux/CI build path for the audio layers.

```sh
brew install xcodegen          # project generator
xcodegen generate              # generates SonicPatch.xcodeproj from project.yml
open SonicPatch.xcodeproj
```

The Xcode project is generated; do not edit `.xcodeproj` files by hand. Edit
[`project.yml`](project.yml) and re-run `xcodegen generate`. See
[Docs/BUILD.md](Docs/BUILD.md) for the full build guide, the C++ interop build
setting, and the GoogleTest/CMake path for engine unit tests.

## Where things live

| Path                 | Owner / contents                                            |
| -------------------- | ----------------------------------------------------------- |
| `SonicPatchApp/`     | SwiftUI menu-bar app (UI, app monitoring, EngineBridge, taps) |
| `SonicPatchEngine/`  | C++17 real-time mixing/effects engine (pImpl `AudioEngine`)  |
| `SonicPatchDriver/`  | HAL virtual-device `.driver` (libASPL), inter-app routing    |
| `SonicPatchInstaller/`| Privileged helper that installs the HAL driver (Phase 5)    |
| `SonicPatchTests/`   | GoogleTest/CMake engine tests (Swift XCTest target: planned) |
| `Vendor/`            | Third-party dependencies (e.g. libASPL)                      |
| `Docs/`              | Architecture, roadmap, build docs                            |

## Branch & PR conventions

- Branch from `main`. Name branches `type/short-description`, e.g.
  `feat/per-app-volume`, `fix/tap-leak-on-quit`, `docs/architecture-ipc`.
  Allowed types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`, `perf`.
- Keep PRs focused and reasonably small. One logical change per PR.
- PR titles follow [Conventional Commits](https://www.conventionalcommits.org/),
  e.g. `feat(tap): create per-process taps on audible transition`.
- Reference the roadmap phase in the PR description where relevant.
- Run the C++ engine tests before pushing (`cmake -S SonicPatchTests -B build
  && cmake --build build && ctest --test-dir build`); they run on any platform.
  There is no macOS CI yet — building the app targets on a Mac before merging
  is strongly encouraged. PRs require at least one review.

## Code style

### Swift

- Follow the [Swift API Design Guidelines](https://www.swift.org/documentation/api-design-guidelines/).
- 4-space indentation; no tabs. Keep lines reasonable (~100 cols).
- Prefer value types (`struct`/`enum`); reserve classes for reference semantics
  (e.g. `ObservableObject` state, Core Audio resource owners).
- UI is SwiftUI. Keep views small and composable; push logic into models and
  services. State flows through `@Published` properties on `ObservableObject`s.
- Mark phase-gated stubs with `// TODO(Phase N):` so they're easy to grep —
  and REMOVE the TODO in the same PR that completes it; stale TODOs poison
  that grep.
- UI placeholders for future phases must be visibly inert: caption them
  (`Text("Phase N — preview")`) and `.disabled(true)` any control that doesn't
  do what it appears to do. Never ship an enabled control bound to mock data.
- Document public types and any non-obvious Core Audio call sites with `///`.

### C++ (engine — C++17)

- C++17. Follow the conventions in `SonicPatchEngine/` (RAII, `std::unique_ptr`,
  `enum class`). Expose a stable C++ API through the pImpl `AudioEngine` facade.
- **Real-time safety is non-negotiable on the audio thread.** Inside any IOProc
  / render callback you must NOT:
  - allocate or free heap memory (`new`/`delete`/`malloc`/`free`, container
    growth, `std::string`),
  - take locks that can block (mutexes); use lock-free SPSC queues / atomics,
  - perform I/O, logging, or syscalls,
  - throw exceptions, or call into Objective-C/Swift ARC.
  Pre-allocate everything; communicate with the UI thread via lock-free
  ring buffers and atomic parameter snapshots. See
  [Docs/ARCHITECTURE.md](Docs/ARCHITECTURE.md#real-time-safety-rules).

## Commit messages

Use Conventional Commits. Keep the subject under ~72 chars; explain the *why* in
the body when it isn't obvious.

## License

By contributing you agree that your contributions are licensed under the MIT
License (see [LICENSE](LICENSE)).
