# SonicPatch Architecture (v0.2)

This is the canonical design document for SonicPatch. It describes the four
layers, the data flow for the core use cases, the process/thread model, the
real-time safety rules, the audio graph, the Swift/C++ interop strategy, IPC,
the built-in DSP set, and the session/preset model.

> **Status:** v0.2 design. Phase 1 (Tap proof-of-concept) is in progress; later
> layers are specified here but stubbed in code. See [ROADMAP.md](ROADMAP.md).

---

## 1. Overview

SonicPatch captures audio from individual running applications using the
Core Audio **Process Tap API** (macOS 14.4+), runs each source through a
real-time C++ mixing/effects engine, and renders the result to the chosen output
device(s). It is a menu-bar (`LSUIElement`) SwiftUI app with a **zero-entitlements
core path**: no kernel extension, no always-on privileged daemon. The only
privileged step is the *optional* HAL virtual-device installer used for inter-app
routing (Phase 5).

---

## 2. The four layers

| Layer | Name | Technology | Responsibility |
| ----- | ---- | ---------- | -------------- |
| 1 | **Capture** | Core Audio Process Tap API | Per-process taps + private aggregate devices with IOProc callbacks deliver each app's audio into the engine. |
| 2 | **Routing** | HAL plugin (user-space virtual device) | An optional `.driver` bundle (libASPL) exposes virtual devices/buses for inter-app routing (send one app's output into another app's input). Phase 5. |
| 3 | **Engine** | C++17 real-time mixing/effects engine | A lock-free audio graph of channel strips: source → effect rack → volume/pan → meter, summed into device sinks. Hosts built-in DSP and (Phase 4) AudioUnits. |
| 4 | **App** | SwiftUI menu-bar app | UI, app monitoring, permission flow, session/preset management, and the `EngineBridge` C++ interop seam. |

```
 ┌──────────────────────────────────────────────────────────────┐
 │ Layer 4: SwiftUI menu-bar app (SonicPatchApp/)                 │
 │   MenuBarView · ChannelStrip · RoutingMatrix · PluginBrowser   │
 │   AppState · AppMonitor · PermissionService · EngineBridge ────┼─┐
 └──────────────────────────────────────────────────────────────┘ │ C++ interop
 ┌──────────────────────────────────────────────────────────────┐ │
 │ Layer 3: C++ engine (SonicPatchEngine/)  sonicpatch::AudioEngine◀┘
 │   Graph: TapSource→Volume→Effect…→Meter→Mixer→DeviceSink       │
 └───────▲───────────────────────────────────────────▲──────────┘
         │ tap audio (IOProc)                         │ device IO
 ┌───────┴────────────────────┐          ┌────────────┴───────────┐
 │ Layer 1: Process Tap API   │          │ Layer 2: HAL .driver    │
 │ (TapManager / ProcessTap)  │          │ (SonicPatchDriver,Ph.5) │
 └────────────────────────────┘          └─────────────────────────┘
```

---

## 3. Data flow

### 3.1 Per-app volume control

1. **Discover** — `AppMonitor` enumerates Core Audio's process objects
   (`kAudioHardwarePropertyProcessObjectList`) and publishes only those whose
   `kAudioProcessPropertyIsRunningOutput` is true, with HAL property listeners
   (process list + per-process) triggering rescans. `NSWorkspace` terminate
   notifications drop entries promptly on quit; launch alone does not make an
   app audible.
2. **Tap** — when an app becomes *audible*, `TapManager` asks `ProcessTap` to
   build a `CATapDescription` for that process, create a process tap
   (`AudioHardwareCreateProcessTap`), wrap it in a *private* aggregate device,
   and install an IOProc.
3. **Strip** — `EngineBridge.createChannelStrip(bundleId)` allocates a
   `StripID`; the IOProc feeds captured frames into that strip's `TapSource`.
4. **Control** — the UI slider calls `EngineBridge.setVolume(strip, db)`. The
   engine applies the change as a smoothed, per-sample gain on the audio thread.
5. **Render** — the strip's output is summed by a `Mixer` into the chosen
   `DeviceSink`, which drives the output device's IOProc.
6. **Meter** — a `Meter` node publishes a `LevelSnapshot` via atomics; the UI
   polls `EngineBridge.getLevel(strip)` at ~60 fps.

### 3.2 Inter-app routing (Phase 5)

1. The HAL `.driver` (libASPL) publishes one or more **virtual output devices**
   ("SonicPatch Bus N"). The user selects such a bus as App A's output device.
2. App A's audio is captured by the virtual device's IOProc and presented to the
   engine as a `TapSource` (no Process Tap needed for routed sources).
3. The engine routes/processes that source and writes it to the virtual device's
   *input* side, which App B opens as its input device — completing app→app
   routing entirely in user space.

---

## 4. Process & thread architecture

| Process / Thread | Role | RT? |
| ---------------- | ---- | --- |
| **App process (main thread)** | SwiftUI, app monitoring, parameter changes (publish snapshots to engine). | No |
| **App process (UI poll timer)** | ~60 fps level metering reads (lock-free). | No |
| **Audio thread(s)** | Core Audio IOProc callbacks (tap capture + device render). Runs the engine graph. | **Yes** |
| **`coreaudiod`** | Hosts the HAL `.driver` virtual device (Phase 5), out-of-process. | Yes |
| **Installer helper** | One-shot privileged process: copies the `.driver`, restarts `coreaudiod`. | No |

The engine is single-process and in-app for capture/processing; only the HAL
virtual device runs inside `coreaudiod`. Parameter updates flow main→audio via
an atomic publish of an immutable graph config; metering flows audio→main via
lock-free reads.

---

## 5. Real-time safety rules

Everything executing inside an IOProc / render callback (the audio thread) must
be **wait-free and allocation-free**. On the audio thread you must NOT:

- allocate/free heap memory (`new`/`delete`/`malloc`/`free`, container growth,
  `std::string`);
- take blocking locks (mutexes) — use lock-free SPSC queues and atomics;
- perform I/O, logging, or syscalls;
- throw exceptions;
- call into Objective-C / Swift (ARC retain/release is not RT-safe).

Mechanisms:

- **Topology delivery:** the control thread builds a new immutable `GraphConfig`
  and publishes it with an atomic pointer swap; the audio thread acquire-loads
  the current config at the top of each callback. Reclamation is **epoch-based**:
  `process()` advances a render epoch, and a retired config (which co-owns its
  nodes via `GraphConfig::ownedNodes`) is freed only once the epoch has advanced
  ≥ 2 past its retirement stamp — proof the audio thread has moved on. When no
  audio thread runs, `drain()` reclaims retired configs (the engine calls it on
  `stop()`). See `Core/AudioGraph.hpp` for the full protocol.
- **Parameter delivery:** individual parameters do NOT go through the config
  swap. Volume/pan/mute and mixer gains use per-node atomics read on the audio
  thread; DSP effect parameters are currently plain fields whose torn reads are
  benign per-field (biquad coefficient sets are an acknowledged TODO — see
  `ParametricEQ.cpp`). A lock-free command queue is planned for Phase 2+.
- **Metering:** `LevelSnapshot` is written with `std::atomic<float>` (relaxed)
  and read by the UI. Strip lookup for `getLevel()` goes through an immutable
  strip→meter snapshot map swapped with `atomic_store`, so 60 fps polling never
  takes the control mutex.
- **Buffer management (Phase 2):** graph-edge buffers will come from the
  pre-allocated `BufferPool` (see `SonicPatchEngine/Core/`); today nodes use
  private staging buffers allocated in `prepare()` and the pool is not yet wired
  to edges. Invariant either way: no allocation after `start()` — which is why
  **format changes are only allowed while stopped** and live nodes are never
  re-prepared (`Node::prepareIfNeeded`).
- **Smoothing:** volume/pan are smoothed per-sample to avoid zipper noise; the
  mixer de-zippers gain changes with a fixed-slope 64-sample ramp that persists
  across block boundaries.

---

## 6. Audio graph & node types

The engine is a directed acyclic graph, topologically sorted once per config
publish (see `Core/TopologicalSort`). A user-facing **channel strip** maps to a
chain of internal nodes:

```
TapSource → [effect rack: ordered EffectNode list] → Volume/Pan → Meter
          → Mixer → DeviceSink
```

> Design intent for later phases: a dedicated input-trim stage and a pre/post-FX
> split around the fader (as in a console strip). Neither exists yet — the
> engine currently has one flat effect rack per strip, and
> `AudioEngine::setInputTrim` records the value without applying it (see the
> stub note in `Public/AudioEngine.hpp`). The UI must not present pre/post slots
> until the engine grows them.

| NodeKind | Purpose |
| -------- | ------- |
| `TapSource` | Pulls frames from a Core Audio process/aggregate tap (or virtual device). |
| `DeviceSink` | Pushes summed frames to an output device IOProc. |
| `Mixer` | Sums N inputs → one output. |
| `Splitter` | Fans one input → N outputs (sends, multi-out). |
| `Volume` | dB gain + constant-power pan + mute (used for trim and main fader). |
| `Meter` | Pass-through peak/RMS metering tap. |
| `Effect` | Built-in DSP or hosted AudioUnit insert. |

(Types mirror `sonicpatch::NodeKind` in `EngineTypes.hpp`.)

---

## 7. Swift / C++ interop strategy

- The engine exposes a **non-virtual pImpl facade** `sonicpatch::AudioEngine`
  (`SonicPatchEngine/Public/AudioEngine.hpp`). Its public header includes only
  dependency-free PODs (`EngineTypes.hpp`) so it imports cleanly into Swift.
- Swift 5.9 **C++ interoperability** is enabled per-target via the build setting
  `SWIFT_OBJC_INTEROP_MODE = objcxx` (the Xcode "C++ / Objective-C++" interop
  mode). See `project.yml`.
- Swift code never touches the C++ types directly throughout the UI. Instead a
  thin Swift wrapper, `EngineBridge`, owns the `AudioEngine` instance and exposes
  Swifty methods (`setVolume(strip:db:)` etc.), translating value types and
  hiding pointer ownership. This isolates the interop seam to one file.
- Non-virtual public API + value-type parameters keep the interop bridge clean;
  virtuality lives entirely in the engine's internal `Node` layer.

---

## 8. IPC (XPC + shared memory)

Two IPC surfaces, both Phase 5+:

| Channel | Transport | Use |
| ------- | --------- | --- |
| **App ↔ Installer helper** | XPC to a privileged `SMAppService`/`launchd` helper, authorized via `AuthorizationRef` | Install/uninstall the HAL `.driver`, restart `coreaudiod`. One-shot, infrequent. |
| **App ↔ HAL virtual device** | Shared-memory ring buffer + a small XPC control channel | Stream routed audio between the in-app engine and the `coreaudiod`-hosted virtual device with minimal copies; XPC carries control/format changes only. |

The hot audio path uses a lock-free shared-memory ring buffer (no XPC per
buffer). XPC is reserved for control-plane messages and the privileged installer.

---

## 9. Built-in DSP

First-party, real-time-safe effects (no AU hosting required). Mirrors
`sonicpatch::BuiltinEffectType`:

| Effect | Notes |
| ------ | ----- |
| Parametric EQ | Multi-band biquad EQ (RBJ cookbook). |
| Compressor | Feed-forward dynamics with attack/release (soft knee: Phase 3). |
| Limiter | Brickwall limiter (look-ahead delay line: Phase 3; currently instant-attack). |
| Noise Gate | Threshold gate (hysteresis + range: Phase 3; currently full mute when closed). |
| High/Low-pass Filter | Tunable HPF/LPF (cascaded biquads). |
| Gain Utility | Gain/pan/polarity/mono-sum utility. |

Each is inserted via `AudioEngine.insertBuiltinEffect(strip, slotIndex, type)`
and parameterized with `setEffectParameter(strip, slot, paramId, value)`.
Third-party **AudioUnit** plugins are hosted in the same slots (Phase 4).

---

## 10. Session & preset model

- A **Session** captures the full app configuration: per-source strip settings
  (trim, volume, pan, mute, output device), effect chains, and routing.
- Sources are keyed by **bundle identifier** so a session re-applies correctly
  across app launches.
- Sessions are `Codable` and serialized to JSON at:

  ```
  ~/Library/Application Support/SonicPatch/Presets/<name>.json
  ```

- **Status:** the `Session` model and JSON persistence exist in
  `SonicPatchApp/Models/Session.swift` but are **not yet wired to the app** —
  loading the last session at launch and auto-saving on change/quit land in
  Phase 6.

---

## 11. Security & entitlements

- **Zero-entitlements core path:** the Process Tap API requires only the user's
  TCC consent for audio capture (`NSAudioCaptureUsageDescription`), prompted on
  first use — no special entitlement, no kext, no daemon.
- The HAL `.driver` install (Phase 5) is the only privileged action and is
  isolated in a one-shot helper authorized via `AuthorizationRef`.
