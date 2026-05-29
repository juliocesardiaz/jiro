//
//  AudioSource.swift
//  SonicPatch
//
//  A user-facing audio-producing application and its per-source configuration.
//

import Foundation
import AppKit

/// One audio-producing application that SonicPatch is managing.
///
/// Identity is the bundle identifier (stable across launches), but `Identifiable`
/// uses a separate stable UUID so SwiftUI lists are well-behaved even if two
/// transient entries momentarily share a bundle id.
struct AudioSource: Identifiable {
    let id: UUID
    let bundleId: String
    var displayName: String

    /// App icon, looked up via `NSWorkspace`. Not persisted.
    var icon: NSImage?

    // Per-source mix settings.
    var volumeDB: Float = 0          ///< Fader gain in dB (0 = unity).
    var muted: Bool = false
    var pan: Float = 0               ///< -1 (L) .. 0 (C) .. +1 (R).
    var inputTrimDB: Float = 0       ///< Pre-FX input trim in dB.

    /// UID of the selected output device, or nil for the system default.
    var outputDeviceUID: String?

    /// The engine strip backing this source, if a tap/strip has been created.
    var stripID: EngineBridge.StripID?

    init(id: UUID = UUID(),
         bundleId: String,
         displayName: String,
         icon: NSImage? = nil) {
        self.id = id
        self.bundleId = bundleId
        self.displayName = displayName
        self.icon = icon
    }

    /// Whether any effects are inserted on this strip (drives the "FX" badge).
    /// TODO(Phase 3): derive from the strip's effect chain in the session config.
    var hasEffects: Bool { false }

    /// Look up the app icon for `bundleId` via `NSWorkspace`.
    static func lookupIcon(forBundleId bundleId: String) -> NSImage? {
        guard let url = NSWorkspace.shared
            .urlForApplication(withBundleIdentifier: bundleId) else { return nil }
        return NSWorkspace.shared.icon(forFile: url.path)
    }
}
