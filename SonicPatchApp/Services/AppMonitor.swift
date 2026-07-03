//
//  AppMonitor.swift
//  SonicPatch
//
//  Tracks applications that are *actually producing audio*. Combines:
//   1. Core Audio's process-object list (`kAudioHardwarePropertyProcessObjectList`)
//      with per-process `kAudioProcessPropertyIsRunningOutput` — the
//      authoritative "this process is playing audio right now" signal.
//   2. NSWorkspace terminate notifications, to drop entries promptly on quit.
//
//  Publishing only audibly-active processes matters: SonicPatch must NOT tap
//  (and thereby mute, via .mutedWhenTapped) every GUI app on the machine at
//  launch — only the ones the user can actually hear.
//
//  Note on bundle ids: the published bundle id is the AUDIO process's bundle id
//  as reported by Core Audio (e.g. "com.apple.WebKit.GPU" for Safari playback,
//  helper bundles for Electron apps) — that is the id ProcessTap must resolve
//  to capture the right process. Display name/icon are resolved best-effort via
//  NSRunningApplication.
//

import Foundation
import AppKit
import Combine
#if canImport(CoreAudio)
import CoreAudio
#endif

/// A running process that is currently producing audio.
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

    /// Apps producing audio right now (empty until something plays).
    @Published private(set) var audibleApps: [AudioApp] = []

    private var observers: [NSObjectProtocol] = []
    private let workspace = NSWorkspace.shared

    // MARK: Lifecycle

    /// Begin observing audible-process transitions and app quits.
    func start() {
        installWorkspaceObservers()
        installAudibleProcessListener()
        rescanAudioProcesses()
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

    // MARK: NSWorkspace (quit signal only)

    private func installWorkspaceObservers() {
        let center = workspace.notificationCenter
        // Launch alone does NOT make an app audible — new apps enter the list
        // via the Core Audio rescan when they start producing output. Quit is
        // observed so dead entries disappear immediately (faster than waiting
        // for the HAL to drop the process object).
        let quit = center.addObserver(
            forName: NSWorkspace.didTerminateApplicationNotification,
            object: nil, queue: .main) { [weak self] note in
                self?.handleQuit(note)
        }
        observers = [quit]
    }

    private func handleQuit(_ note: Notification) {
        guard let app = note.userInfo?[NSWorkspace.applicationUserInfoKey]
                as? NSRunningApplication else { return }
        let pid = app.processIdentifier
        audibleApps.removeAll { $0.pid == pid }
    }

    // MARK: Core Audio audible-process detection

#if canImport(CoreAudio)

    /// Listener installed on the HAL system object; fires when the process
    /// object list changes. Per-process IsRunningOutput listeners are installed
    /// on every known process object so start/stop of playback also triggers a
    /// rescan.
    private var listenerBlock: AudioObjectPropertyListenerBlock?
    private var processListenerObjects: Set<AudioObjectID> = []

    private static var processListAddress = AudioObjectPropertyAddress(
        mSelector: kAudioHardwarePropertyProcessObjectList,
        mScope: kAudioObjectPropertyScopeGlobal,
        mElement: kAudioObjectPropertyElementMain)

    private static var isRunningOutputAddress = AudioObjectPropertyAddress(
        mSelector: kAudioProcessPropertyIsRunningOutput,
        mScope: kAudioObjectPropertyScopeGlobal,
        mElement: kAudioObjectPropertyElementMain)

    private func installAudibleProcessListener() {
        let block: AudioObjectPropertyListenerBlock = { [weak self] _, _ in
            DispatchQueue.main.async { self?.rescanAudioProcesses() }
        }
        listenerBlock = block
        AudioObjectAddPropertyListenerBlock(
            AudioObjectID(kAudioObjectSystemObject),
            &Self.processListAddress,
            DispatchQueue.main,
            block)
    }

    private func removeAudibleProcessListener() {
        guard let block = listenerBlock else { return }
        AudioObjectRemovePropertyListenerBlock(
            AudioObjectID(kAudioObjectSystemObject),
            &Self.processListAddress,
            DispatchQueue.main,
            block)
        for obj in processListenerObjects {
            AudioObjectRemovePropertyListenerBlock(
                obj, &Self.isRunningOutputAddress, DispatchQueue.main, block)
        }
        processListenerObjects.removeAll()
        listenerBlock = nil
    }

    /// Enumerate Core Audio's process objects and publish those currently
    /// running audio output. Also (re)installs per-process listeners so that a
    /// process starting/stopping playback triggers the next rescan.
    func rescanAudioProcesses() {
        var apps: [AudioApp] = []
        var seenBundleIds = Set<String>()

        for processObject in Self.copyProcessObjectList() {
            // Per-process listener (idempotent per object id).
            if let block = listenerBlock,
               !processListenerObjects.contains(processObject) {
                AudioObjectAddPropertyListenerBlock(
                    processObject, &Self.isRunningOutputAddress,
                    DispatchQueue.main, block)
                processListenerObjects.insert(processObject)
            }

            guard Self.processIsRunningOutput(processObject),
                  let bundleId = Self.processBundleId(processObject),
                  !bundleId.isEmpty,
                  bundleId != Bundle.main.bundleIdentifier, // never tap ourselves
                  !seenBundleIds.contains(bundleId)
            else { continue }
            seenBundleIds.insert(bundleId)

            let pid = Self.processPID(processObject) ?? -1
            let running = NSRunningApplication(processIdentifier: pid)
            apps.append(AudioApp(
                bundleId: bundleId,
                displayName: running?.localizedName ?? bundleId,
                pid: pid,
                icon: running?.icon))
        }

        if apps != audibleApps { audibleApps = apps }
    }

    // MARK: Core Audio property helpers

    private static func copyProcessObjectList() -> [AudioObjectID] {
        var addr = processListAddress
        var size: UInt32 = 0
        guard AudioObjectGetPropertyDataSize(
            AudioObjectID(kAudioObjectSystemObject), &addr, 0, nil, &size) == noErr,
            size > 0 else { return [] }
        var list = [AudioObjectID](
            repeating: AudioObjectID(kAudioObjectUnknown),
            count: Int(size) / MemoryLayout<AudioObjectID>.size)
        guard AudioObjectGetPropertyData(
            AudioObjectID(kAudioObjectSystemObject), &addr, 0, nil, &size, &list) == noErr
        else { return [] }
        return list
    }

    private static func processIsRunningOutput(_ object: AudioObjectID) -> Bool {
        var addr = isRunningOutputAddress
        var value: UInt32 = 0
        var size = UInt32(MemoryLayout<UInt32>.size)
        guard AudioObjectGetPropertyData(object, &addr, 0, nil, &size, &value) == noErr
        else { return false }
        return value != 0
    }

    private static func processBundleId(_ object: AudioObjectID) -> String? {
        var addr = AudioObjectPropertyAddress(
            mSelector: kAudioProcessPropertyBundleID,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMain)
        var value = "" as CFString
        var size = UInt32(MemoryLayout<CFString>.size)
        let status = withUnsafeMutablePointer(to: &value) {
            AudioObjectGetPropertyData(object, &addr, 0, nil, &size, $0)
        }
        guard status == noErr else { return nil }
        return value as String
    }

    private static func processPID(_ object: AudioObjectID) -> pid_t? {
        var addr = AudioObjectPropertyAddress(
            mSelector: kAudioProcessPropertyPID,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMain)
        var value: pid_t = -1
        var size = UInt32(MemoryLayout<pid_t>.size)
        guard AudioObjectGetPropertyData(object, &addr, 0, nil, &size, &value) == noErr
        else { return nil }
        return value
    }

#else // !canImport(CoreAudio) — portable stubs so the file parses off-Apple.

    private func installAudibleProcessListener() {}
    private func removeAudibleProcessListener() {}
    func rescanAudioProcesses() {}

#endif
}
