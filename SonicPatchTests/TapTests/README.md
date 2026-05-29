# TapTests

These tests exercise the Core Audio Tap capture path (`TapSourceNode` + the
`AudioHardwareTap` / process-tap APIs introduced in macOS 14.4).

They are **not runnable in CI / offline** because they require:

- real macOS hardware (the Tap APIs are not in the iOS/Linux SDKs),
- a running `coreaudiod`,
- the **Audio Recording / system audio capture permission** granted to the test
  host (TCC prompt), and
- at least one live audio-producing process to tap.

For that reason the host-side `SonicPatchTests/CMakeLists.txt` deliberately
excludes anything that links Core Audio. Tap behaviour is validated manually on
a Mac, or in a gated macOS CI lane with the permission pre-granted via an MDM
profile / `tccutil` reset + UI automation.

The portable parts of `TapSourceNode` (the pre-allocated staging buffer drain in
`process()` and `depositFromTap()`) are covered indirectly by the engine graph
tests, which feed it deterministic data without Core Audio.
