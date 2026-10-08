# SonicPatch initial demo

A native macOS mixer inspired by SoundSource's per-app controls. This is an
initial playable slice, with its own SwiftUI entry point and a small C++ render
processor. It does not claim SoundSource feature parity.

## Run on your Mac

Requirements: Apple Silicon (M1/M2/M3/M4 or later), macOS 14.4+, and an Apple SDK
that contains the Core Audio Process Tap API (Xcode 15.3+ or corresponding
Command Line Tools). No third-party libraries, account credentials, virtual
driver, administrator helper, or Homebrew packages are needed by this target.

1. Check out the `feat/native-demo` branch of this repository.
2. If Apple's developer tools are not installed, run `xcode-select --install`
   and finish the macOS installation dialog.
3. From the repository folder, run:

   ```sh
   bash scripts/build-demo.sh
   ```

4. Look for the SonicPatch mixer window or slider icon in the menu bar.
5. Play audio in Music, Spotify, or a browser. Click **Start mixing**.
6. Grant the system-audio capture permission macOS requests. If necessary,
   open System Settings → Privacy & Security → Screen & System Audio Recording
   (wording varies by macOS), allow SonicPatch Demo, then quit and reopen it.

The build script creates `build/SonicPatch Demo.app`, signs it ad hoc for local
use, validates the signature, and smoke-tests executable loading before opening
the UI. It does not change your system default audio device. Close the window
to keep the menu-bar app running; use the menu's Quit item to exit completely.

The GitHub Actions `Native demo` workflow also packages an arm64 app artifact
after a successful Mac build. It is ad-hoc signed, **not Developer-ID signed or
notarized**. Building locally is the preferred first-run route; downloaded apps
may need macOS's normal Open Anyway approval. Do not disable Gatekeeper.

## What this demo does

- Finds processes that play audio and presents one row per audio-process bundle.
- Controls 0–100% volume, mute, and left/right stereo balance independently.
- Shows post-fader peak meters; a muted source meters silence after its short fade.
- Routes each captured source to a selected stereo output, or follows the system
  default. Device changes are noticed within about one second.
- Saves each bundle's volume, mute, balance, and output UID in local preferences.
- Keeps taps through playback pauses, and rebuilds them when helper processes or
  devices change. Some browsers expose helper names instead of the parent app.
- Releases captures on **Restore audio**, quit, sleep, setup errors, or a stalled
  callback. A failed source gets an error and a Retry button; it is not silently
  retried indefinitely. After sleep, press Start mixing again.
- Processes audio locally. No recording, network service, telemetry, or upload.

Expand an app row with its chevron to choose an output or adjust balance.
Controls are disabled until that source is successfully captured.

## Playback acceptance check — still requires a real Mac

Automated builds cannot grant your capture permission or establish that a real
speaker produced sound. Record the Mac model, macOS version, output device, and
result when performing this check. Do not call hardware playback verified until
these steps pass.

1. Start two apps playing distinct audio. Confirm both rows appear, meters move,
   and playback is heard once (no doubled signal).
2. Set the first app to 25%. Only that app should become quieter. Mute it, then
   unmute; the second app should remain unchanged.
3. Sweep balance left/right on headphones; Center should restore both channels
   at their original level. At 100%, center balance is unity, without a 3 dB dip.
4. Route one app to another stereo output. Confirm the other app stays on its
   own output. Test following a change to the system default, and unplug an
   explicitly selected output. A disconnected output should release capture
   and show an error. Choose a connected output and Retry.
5. Pause/resume playback; quit/relaunch a source app. Settings should persist,
   and mute must never trigger capture removal merely because it is silent.
6. Click Restore audio, then quit SonicPatch. Both source apps must return to
   their normal playback. Reopen SonicPatch; settings should persist but mixing
   must stay off until Start mixing is clicked.
7. Deny capture permission once. Confirm an actionable error or stalled-callback
   message appears and capture is released; grant permission, relaunch, and retry.

Start with built-in speakers or wired headphones. Bluetooth, AirPlay, USB
interfaces, protected/DRM audio, and unusual device layouts need separate Mac
verification. The demo accepts packed native-endian Float32 stereo taps and
stereo-or-wider Float32 outputs. It plays into the first two output channels;
other output channels are zeroed. Unsupported formats are rejected with an error.

## Engineering notes

`Demo/Audio/AudioControl.cpp` contains the actual audio callback and processor.
A private aggregate combines each process tap and its chosen hardware output.
The physical output is the clock source; tap drift compensation is enabled.
On duplex devices, physical input channels are skipped so microphone samples
are never mistaken for captured application audio. The expected channel layout
and actual callback stream formats are checked before playback starts.

Each tap owns a separate C++ context and a single render thread. Controls and
meters use lock-free atomics. The callback contains no Swift/Objective-C calls,
allocation, locks, logging, or file/network I/O. Gain changes use a 5 ms ramp
whose state persists across callback boundaries. Stop/destroy the IOProc before
freeing its context. A nonblocking callback counter detects stalled I/O.

The original `SonicPatchEngine/` graph remains available for future effect chains
and routing. It is not used by this small target: its graph edges still lack
live buffers, and concurrently rendering that shared graph from independent app
callbacks would be incorrect. This separate target provides audible controls
without presenting unfinished effect/driver features as working controls.

Not included: EQ/effects, AU hosting, virtual buses, system input controls,
multi-output groups, volume boost, global hotkeys, or notarized distribution.

## Tests and build automation

```sh
bash scripts/test-demo.sh
SANITIZE=1 bash scripts/test-demo.sh
bash scripts/build-demo.sh --no-open  # macOS only
```

Audio tests run without third-party dependencies on Linux or macOS. They check
actual rendered samples for attenuation, mute, unity balance, smoothing across
buffer sizes, planar/interleaved layouts, duplex input offsets, silence handling,
independent source states, nonfinite inputs, and allocation-free rendering.
The Mac workflow runs these tests normally and under ASan/UBSan, then builds,
signs, smoke-launches, and packages the native SwiftUI app.

API references:
- [Apple: Capturing system audio with Core Audio taps](https://developer.apple.com/documentation/coreaudio/capturing-system-audio-with-core-audio-taps)
- [Apple: CATapMuteBehavior](https://developer.apple.com/documentation/coreaudio/catapmutebehavior)
- [SoundSource product reference](https://www.rogueamoeba.com/soundsource/)
