//
//  AppMonitor.swift
//  SonicPatch
//
//  Tracks audio-producing applications. Combines two signals:
//   1. NSWorkspace launch/terminate notifications (pure Swift — wired for real).
//   2. The Core Audio HAL property `kAudioHardwarePropertyProcessIsAudible`
//      (stubbed — requires Core Audio, unavailable in this environment).
//

import Foundation
import AppKit
import Combine

/// A running application that may produce audio.
struct AudioApp: Identifiable, Equatable {
    var id: String { bundleId }
    let bundleId: String
    let displayName: String
    let pid: pid_t
    var icon: NSImage?

    static func == (lhs: AudioApp, rhs: AudioApp) -> Bool {
        lhs.bundleId == rhs.bundleId && lhs.pid == rhs.pid
    }
}

/// Publishes the set of currently audible applications.
final class AppMonitor: ObservableObject {

    /// Apps considered audio-producing right now.
    @Published private(set) var audibleApps: [AudioApp] = []

    private var observers: [NSObjectProtocol] = []
    private let workspace = NSWorkspace.shared

    // MARK: Lifecycle

    /// Begin observing app launch/quit and audible-process transitions.
    func start() {
        seedFromRunningApps()
        installWorkspaceObservers()
        installAudibleProcessListener()
    }

    /// Stop observing and tear down listeners.
    func stop() {
        for token in observers {
            workspace.notificationCenter.removeObserver(token)
        }
        observers.removeAll()
        removeAudibleProcessListener()
    }

    deinit { stop() }

    // MARK: NSWorkspace (real)

    /// Populate the initial list from currently running, non-background apps.
    private func seedFromRunningApps() {
        let apps = workspace.runningApplications
            .filter { $0.activationPolicy == .regular }
            .compactMap { Self.makeAudioApp(from: $0) }
        audibleApps = apps
    }

    private func installWorkspaceObservers() {
        let center = workspace.notificationCenter

        let launch = center.addObserver(
            forName: NSWorkspace.didLaunchApplicationNotification,
            object: nil, queue: .main) { [weak self] note in
                self?.handleLaunch(note)
        }
        let quit = center.addObserver(
            forName: NSWorkspace.didTerminateApplicationNotification,
            object: nil, queue: .main) { [weak self] note in
                self?.handleQuit(note)
        }
        observers = [launch, quit]
    }

    private func handleLaunch(_ note: Notification) {
        guard let app = note.userInfo?[NSWorkspace.applicationUserInfoKey]
                as? NSRunningApplication,
              let model = Self.makeAudioApp(from: app) else { return }
        // TODO(Phase 2): don't add until the process is actually audible; defer
        // tap creation to the audible-process listener below.
        if !audibleApps.contains(where: { $0.bundleId == model.bundleId }) {
            audibleApps.append(model)
        }
    }

    private func handleQuit(_ note: Notification) {
        guard let app = note.userInfo?[NSWorkspace.applicationUserInfoKey]
                as? NSRunningApplication,
              let bundleId = app.bundleIdentifier else { return }
        audibleApps.removeAll { $0.bundleId == bundleId }
    }

    private static func makeAudioApp(from app: NSRunningApplication) -> AudioApp? {
        guard let bundleId = app.bundleIdentifier else { return nil }
        let name = app.localizedName ?? bundleId
        return AudioApp(bundleId: bundleId,
                        displayName: name,
                        pid: app.processIdentifier,
                        icon: app.icon)
    }

    // MARK: Core Audio audible-process listener (stub)

    /// Install a HAL property listener on `kAudioHardwarePropertyProcessIsAudible`
    /// so we react when a process starts/stops producing audio. This is the
    /// authoritative signal for when to create or defer a tap.
    ///
    /// TODO(Phase 2): implement with Core Audio:
    ///   var addr = AudioObjectPropertyAddress(
    ///       mSelector: kAudioHardwarePropertyProcessIsAudible,
    ///       mScope: kAudioObjectPropertyScopeGlobal,
    ///       mElement: kAudioObjectPropertyElementMain)
    ///   AudioObjectAddPropertyListenerBlock(AudioObjectID(kAudioObjectSystemObject),
    ///                                        &addr, queue) { _, _ in /* re-scan */ }
    /// Then translate audio process objects (kAudioProcessPropertyBundleID,
    /// kAudioProcessPropertyIsRunningInput/Output) into AudioApp entries.
    private func installAudibleProcessListener() {
        // No-op in the stub build (no Core Audio here).
    }

    private func removeAudibleProcessListener() {
        // TODO(Phase 2): AudioObjectRemovePropertyListenerBlock(...)
    }
}
