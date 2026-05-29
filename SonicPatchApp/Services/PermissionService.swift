//
//  PermissionService.swift
//  SonicPatch
//
//  Manages the audio-capture (TCC) permission required by the Process Tap API.
//
//  The Process Tap API is gated by the user's audio-capture consent, declared by
//  `NSAudioCaptureUsageDescription` in Info.plist. On first attempt to create a
//  tap, macOS presents the system permission prompt; the result is recorded in
//  TCC and reflected here.
//

import Foundation

/// Current audio-capture authorization state.
enum AudioCaptureAuthorization {
    case notDetermined   ///< Never asked; will prompt on first capture.
    case authorized
    case denied
    case restricted      ///< Blocked by MDM / parental controls.
}

/// Front-end for the audio-capture TCC permission.
final class PermissionService {

    /// Current authorization status.
    ///
    /// TODO(Phase 1): query the real TCC state. There is no public API to read
    /// the audio-capture status directly; in practice we infer it from the
    /// success/failure of `AudioHardwareCreateProcessTap` and cache the result.
    func currentStatus() -> AudioCaptureAuthorization {
        // TODO(Phase 1): return cached/inferred status.
        .notDetermined
    }

    /// Trigger the permission flow by attempting a capture. macOS shows the
    /// prompt (using `NSAudioCaptureUsageDescription`) the first time.
    ///
    /// - Parameter completion: called with the resolved authorization.
    func requestAccess(completion: @escaping (AudioCaptureAuthorization) -> Void) {
        // TODO(Phase 1): attempt a throwaway tap to provoke the TCC prompt, then
        // map the outcome:
        //   - success            -> .authorized
        //   - tap creation error  -> .denied
        // Cache the result and notify the UI.
        completion(.notDetermined)
    }

    /// Open System Settings at Privacy & Security ▸ (audio capture) so the user
    /// can change a previously-denied decision.
    func openSystemSettings() {
        // TODO(Phase 1): open the appropriate Privacy pane, e.g.
        //   x-apple.systempreferences:com.apple.preference.security?Privacy
        // (audio capture pane identifier TBD on target OS).
    }
}
