# SonicPatch Roadmap

Six phases from proof-of-concept to launch. Each phase has a clear goal and a set
of deliverables. The canonical design behind these phases is in
[ARCHITECTURE.md](ARCHITECTURE.md).

> **Current status:** **Phase 1 — Tap proof-of-concept (code complete, pending
> on-device verification).** The full capture path is implemented:
> `ProcessTap` performs real Core Audio process tapping (`CATapDescription` →
> `AudioHardwareCreateProcessTap` → private aggregate device combining the tap and
> the default output → pass-through `AudioDeviceIOProcID` with peak metering),
> `TapManager` does defer-until-audible retry, and `EngineBridge` calls the real
> C++ `sonicpatch::AudioEngine` over Swift C++ interop. **This has not yet been
> built or run on a Mac** (no Xcode/Core Audio toolchain in CI) — that's the one
> remaining step to close Phase 1. Build with `xcodegen generate` and verify on
> Apple Silicon (macOS 14.4+); see [BUILD.md](BUILD.md).

---

## Phase 1 — Tap proof-of-concept *(code complete, pending on-device verification)*

**Goal:** prove the Core Audio Process Tap API path end-to-end: capture one
running app's audio and render it to an output device.

**Deliverables:**
- TCC audio-capture permission flow (`PermissionService`). — *stub; wired in Phase 2*
- `ProcessTap`: `CATapDescription` → `AudioHardwareCreateProcessTap` → private
  aggregate device (`kAudioAggregateDeviceTapListKey`,
  `kAudioAggregateDeviceIsPrivateKey`) → `AudioDeviceCreateIOProcIDWithBlock`. — ✅ **implemented**
- `TapManager`: create/destroy taps by bundle id, retry-on-failure / defer-until-
  audible logic. — ✅ **implemented** (2s retry poll)
- Minimal pass-through: tapped input copied straight to the output device inside a
  single IOProc on one clock. — ✅ **implemented** (routing through the C++
  `TapSource`/`DeviceSink` nodes lands in Phase 2)
- Read tap format via `kAudioTapPropertyFormat`. — ✅ **implemented**
- Live peak meter from the IOProc, surfaced in `MenuBarView`. — ✅ **implemented**

**Remaining to close Phase 1:** generate the Xcode project (`xcodegen generate`),
build on Apple Silicon (macOS 14.4+), grant the capture permission, and confirm an
app's audio is heard through SonicPatch with a moving meter.

## Phase 2 — Per-app volume

**Goal:** independent real-time volume/mute/pan per source.

**Deliverables:**
- Channel-strip `Volume` node with smoothed dB gain, mute, constant-power pan.
- `MenuBarView` wired to live sources: per-app volume slider, mute, peak meter.
- `AppMonitor` driving the audible-app list from `NSWorkspace` +
  `kAudioHardwarePropertyProcessIsAudible`.
- Per-source output device selection.

## Phase 3 — Built-in DSP

**Goal:** first-party, RT-safe effects in the channel strip.

**Deliverables:**
- DSP set: Parametric EQ, Compressor, Limiter, Noise Gate, HP/LP Filter, Gain.
- `insertBuiltinEffect` / `setEffectParameter` / `removeEffect` wired through.
- `ChannelStripView`: input trim, 4 pre-FX + 4 post-FX slots, volume/pan, meter.

## Phase 4 — AU hosting

**Goal:** host third-party Audio Units (v2/v3) in any insert slot.

**Deliverables:**
- `AudioUnitHost` in the engine (`insertEffect(strip, slot, AudioComponentDescription)`).
- `PluginBrowserView`: searchable AU component list; insert into a slot.
- AU parameter UI / generic view hosting; state persistence in sessions.

## Phase 5 — HAL plugin & inter-app routing

**Goal:** route any source to any output/bus and app→app, via a user-space HAL
virtual device.

**Deliverables:**
- `SonicPatchDriver` libASPL-based `.driver` exposing virtual buses.
- `InstallerService` + privileged `InstallerHelper`: install/uninstall the
  `.driver` to `/Library/Audio/Plug-Ins/HAL/`, restart `coreaudiod`
  (`AuthorizationRef`).
- App ↔ HAL shared-memory ring buffer + XPC control channel.
- `RoutingMatrixView`: sources × buses × destinations matrix.

## Phase 6 — Polish & launch

**Goal:** ship a stable 1.0.

**Deliverables:**
- Session/preset management UI; onboarding & permission UX.
- Performance/latency tuning; CPU and glitch monitoring.
- Crash/error handling, logging, diagnostics export.
- App icon, notarization/signing, docs, website, release artifacts.
