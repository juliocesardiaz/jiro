//
//  InstallerService.swift
//  SonicPatch
//
//  Installs/uninstalls the HAL virtual-device `.driver` used for inter-app
//  routing (Phase 5). This is the only privileged operation in SonicPatch; the
//  core capture path needs no elevated rights.
//
//  The actual privileged work runs in a separate one-shot helper
//  (SonicPatchInstaller/) authorized via AuthorizationRef. This service is the
//  app-side façade that talks to it.
//

import Foundation

/// Result of an install/uninstall attempt.
enum InstallerResult {
    case success
    case userCancelled
    case failed(String)
}

/// App-side façade for the privileged HAL driver installer.
final class InstallerService {

    /// Standard HAL plug-in directory the `.driver` is copied into.
    static let halPlugInsPath = "/Library/Audio/Plug-Ins/HAL/"

    /// Whether the SonicPatch HAL driver appears to be installed.
    func isDriverInstalled() -> Bool {
        let dest = Self.halPlugInsPath + "SonicPatch.driver"
        return FileManager.default.fileExists(atPath: dest)
    }

    /// Install the bundled `.driver` and restart `coreaudiod`.
    ///
    /// Documented steps (executed by the privileged helper):
    ///   1. Acquire an `AuthorizationRef` with admin rights (the user is
    ///      prompted by the system once).
    ///   2. Copy `SonicPatch.driver` from the app bundle into
    ///      `/Library/Audio/Plug-Ins/HAL/`, fixing ownership (root:wheel) and
    ///      permissions.
    ///   3. Restart Core Audio: `launchctl kickstart -k system/com.apple.audio.coreaudiod`
    ///      (equivalently `killall coreaudiod`) so the HAL re-scans plug-ins.
    ///   4. Verify the virtual device enumerates, then report success.
    ///
    /// TODO(Phase 5): implement via SMAppService/launchd helper + XPC; pass the
    /// source bundle URL and await the helper's result.
    func installDriver(completion: @escaping (InstallerResult) -> Void) {
        // TODO(Phase 5): connect to the privileged helper and invoke install.
        completion(.failed("HAL driver installation is not implemented yet (Phase 5)."))
    }

    /// Uninstall the `.driver` and restart `coreaudiod`.
    ///
    /// TODO(Phase 5): mirror `installDriver` — remove the bundle from the HAL
    /// directory under authorization, then kickstart coreaudiod.
    func uninstallDriver(completion: @escaping (InstallerResult) -> Void) {
        // TODO(Phase 5): connect to the privileged helper and invoke uninstall.
        completion(.failed("HAL driver removal is not implemented yet (Phase 5)."))
    }
}
