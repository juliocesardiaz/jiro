//
//  SonicPatchApp.swift
//  SonicPatch
//
//  Application entry point. SonicPatch runs as a menu-bar agent
//  (LSUIElement = true in Info.plist), so the primary surface is a
//  `MenuBarExtra` popover. A `Settings`/`Window` scene hosts the larger
//  channel-strip / routing UI when the user opens it from the menu.
//

import SwiftUI

@main
struct SonicPatchApp: App {
    /// Shared, observable application state. Owns the `EngineBridge` and the
    /// list of audio sources, and drives the metering poll timer.
    @StateObject private var appState = AppState()

    var body: some Scene {
        // Primary surface: a menu-bar item with a popover-style content view.
        MenuBarExtra("SonicPatch", systemImage: "slider.horizontal.3") {
            MenuBarView()
                .environmentObject(appState)
        }
        .menuBarExtraStyle(.window) // popover-style window, not a plain menu

        // Secondary window: the full mixer / channel-strip / routing UI.
        // Opened on demand (e.g. from a "Open Mixer" button in the menu bar).
        Window("SonicPatch Mixer", id: "mixer") {
            MixerWindowView()
                .environmentObject(appState)
                .frame(minWidth: 720, minHeight: 480)
        }
        .windowResizability(.contentMinSize)

        // App settings.
        Settings {
            SettingsView()
                .environmentObject(appState)
        }
    }
}

/// Container for the main mixer window: a routing matrix on top of the per-source
/// channel strips. Phase-gated; individual views are stubs for now.
struct MixerWindowView: View {
    @EnvironmentObject private var appState: AppState

    var body: some View {
        VSplitView {
            RoutingMatrixView()
                .frame(minHeight: 160)
            ChannelStripView(source: appState.selectedSource)
                .frame(minHeight: 280)
        }
    }
}

/// Minimal settings surface. Expanded in Phase 6 (onboarding, permissions,
/// preset management, diagnostics).
struct SettingsView: View {
    @EnvironmentObject private var appState: AppState

    var body: some View {
        Form {
            Section("Engine") {
                LabeledContent("Status",
                               value: appState.isRunning ? "Running" : "Stopped")
                LabeledContent("Sample rate",
                               value: "\(Int(appState.sampleRate)) Hz")
            }
            // TODO(Phase 6): permission status, default output, preset library,
            // diagnostics export.
        }
        .formStyle(.grouped)
        .frame(width: 420, height: 240)
    }
}
