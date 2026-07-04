# SonicPatchInstaller

A small **privileged helper** that installs and uninstalls the SonicPatch HAL
virtual-device driver. This is the *only* privileged component in SonicPatch; the
core audio-capture path (Phases 1–4) runs entirely unprivileged.

> **Phase:** 5 (HAL plugin & inter-app routing). This directory is a documented
> skeleton today, and it is deliberately **not part of any Xcode target** — the
> code here is never compiled until Phase 5 adds a `SonicPatchInstaller` target
> to `project.yml`. Expect it to need type-check fixes when that happens.

## Why a separate helper?

Installing a Core Audio HAL plug-in requires writing into a system-owned
directory and restarting the audio daemon — both privileged operations. macOS
best practice is to keep that work in a tiny, auditable, one-shot helper that the
main (sandboxed, unprivileged) app talks to over XPC, rather than elevating the
whole app.

## What it will do (Phase 5)

1. Acquires an `AuthorizationRef` with admin rights (the system prompts the user
   once).
2. Copies `SonicPatch.driver` from the app bundle into
   `/Library/Audio/Plug-Ins/HAL/`, setting ownership `root:wheel` and correct
   permissions.
3. Restarts Core Audio so the HAL re-scans plug-ins:
   `launchctl kickstart -k system/com.apple.audio.coreaudiod`
   (equivalently `killall coreaudiod`).
4. Verifies the virtual device enumerates and reports the result back to the
   app. *(This step exists only here, not yet in the skeleton code.)*

Uninstall reverses step 2 (removes the bundle) and repeats steps 1, 3.

## Integration

- Registered/managed via `SMAppService` (or a `launchd` privileged helper) and
  invoked from the app's `InstallerService` over XPC.
- See `SonicPatchApp/Services/InstallerService.swift` for the app-side façade and
  `Docs/ARCHITECTURE.md` (IPC section) for the trust boundary.
