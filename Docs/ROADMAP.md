# SonicPatch Roadmap

Six phases from proof-of-concept to launch. Each phase has a clear goal and a set
of deliverables. The canonical design behind these phases is in
[ARCHITECTURE.md](ARCHITECTURE.md).

> **Current status:** **Phase 1 — Tap proof-of-concept.** Project scaffolding is
> committed: XcodeGen `project.yml`, the SwiftUI app skeleton (menu-bar shell,
> `AppState`, `EngineBridge` stub, `TapManager`/`ProcessTap` structure), the C++
> engine facade, and these docs. Next up: get a single live per-process tap
> delivering audio through to an output device.

---

## Phase 1 — Tap proof-of-concept *(in progress)*

**Goal:** prove the Core Audio Process Tap API path end-to-end: capture one
running app's audio and render it to an output device.

**Deliverables:**
- TCC audio-capture permission flow (`PermissionService`).
- `ProcessTap`: `CATapDescription` → `AudioHardwareCreateProcessTap` → private
  aggregate device (`kAudioAggregateDeviceTapListKey`,
  `kAudioAggregateDeviceIsPrivateKey`) → `AudioDeviceCreateIOProcIDWithBlock`.
- `TapManager`: create/destroy taps by bundle id, retry-on-failure / defer-until-
  audible logic.
- Minimal pass-through: tap → engine `TapSource` → `DeviceSink`.
- Read tap format via `kAudioTapPropertyFormat`.

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
