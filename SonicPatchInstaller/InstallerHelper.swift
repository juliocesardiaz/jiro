//
//  InstallerHelper.swift
//  SonicPatchInstaller
//
//  Privileged helper entry point. Installs/uninstalls the SonicPatch HAL
//  `.driver` and restarts coreaudiod. Runs as a one-shot, privileged process
//  invoked by the main app over XPC (authorized via AuthorizationRef).
//
//  Phase 5 — documented skeleton. The Security / ServiceManagement / file-system
//  calls are stubbed but the control flow and steps are accurate.
//

import Foundation

/// Where HAL plug-ins live on disk.
private let kHALPlugInsDirectory = "/Library/Audio/Plug-Ins/HAL/"
private let kDriverBundleName = "SonicPatch.driver"

/// Errors the helper can report back to the app.
enum InstallerHelperError: Error {
    case notAuthorized
    case sourceMissing
    case copyFailed(String)
    case coreAudioRestartFailed(String)
}

/// The helper's operations. In a real build these are exposed as an
/// `@objc` XPC protocol implemented by this class.
protocol InstallerHelperProtocol {
    func install(driverBundlePath: String) throws
    func uninstall() throws
}

/// Concrete helper implementation.
final class InstallerHelper: InstallerHelperProtocol {

    /// Install the `.driver` from `driverBundlePath` into the HAL directory and
    /// restart Core Audio.
    func install(driverBundlePath: String) throws {
        try requireAuthorization()

        let source = URL(fileURLWithPath: driverBundlePath)
        guard FileManager.default.fileExists(atPath: source.path) else {
            throw InstallerHelperError.sourceMissing
        }
        let dest = URL(fileURLWithPath: kHALPlugInsDirectory)
            .appendingPathComponent(kDriverBundleName)

        // TODO(Phase 5):
        //   1. Remove any existing bundle at `dest`.
        //   2. Copy `source` -> `dest`.
        //   3. chown root:wheel and chmod the tree appropriately.
        //   4. restartCoreAudio()
        do {
            // try FileManager.default.copyItem(at: source, to: dest)
        } catch {
            throw InstallerHelperError.copyFailed(error.localizedDescription)
        }
        try restartCoreAudio()
    }

    /// Remove the installed `.driver` and restart Core Audio.
    func uninstall() throws {
        try requireAuthorization()
        let dest = URL(fileURLWithPath: kHALPlugInsDirectory)
            .appendingPathComponent(kDriverBundleName)
        // TODO(Phase 5): if exists, try FileManager.default.removeItem(at: dest)
        _ = dest
        try restartCoreAudio()
    }

    // MARK: Internals

    /// Verify the caller is authorized (AuthorizationRef passed over XPC).
    private func requireAuthorization() throws {
        // TODO(Phase 5): validate the AuthorizationExternalForm handed in by the
        // app and confirm it holds the admin right
        // (kAuthorizationRightExecute / a custom right) via
        // AuthorizationCopyRights. Throw .notAuthorized otherwise.
    }

    /// Restart the Core Audio daemon so the HAL re-scans plug-ins.
    private func restartCoreAudio() throws {
        // TODO(Phase 5): exec
        //   /bin/launchctl kickstart -k system/com.apple.audio.coreaudiod
        // and surface a non-zero exit as .coreAudioRestartFailed.
    }
}

// MARK: - Helper main

// In a real privileged helper this sets up an NSXPCListener, vends an
// InstallerHelper instance per connection, and runs the run loop:
//
//   let delegate = HelperListenerDelegate()
//   let listener = NSXPCListener(machServiceName: "com.sonicpatch.installer")
//   listener.delegate = delegate
//   listener.resume()
//   RunLoop.current.run()
//
// Kept as documentation so the file is a faithful skeleton without pulling in
// ServiceManagement here. (Phase 5)
