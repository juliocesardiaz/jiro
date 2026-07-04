//
//  TapManager.swift
//  SonicPatch
//
//  High-level manager for process taps. Tracks active taps by bundle id, wires
//  each tap to an engine strip, and implements the retry / defer-until-audible
//  policy described in the architecture (a process can't be tapped until it is
//  actually producing audio).
//

import Foundation

/// Creates, tracks, and destroys `ProcessTap`s on behalf of `AppState`.
@MainActor
final class TapManager {

    private let engine: EngineBridge

    /// Active taps keyed by bundle identifier.
    private var taps: [String: ProcessTap] = [:]

    /// Bundle ids whose activation failed and are awaiting retry (e.g. not yet
    /// audible). Reattempted when the audible-process listener fires.
    private var pending: [String: PendingTap] = [:]

    private struct PendingTap {
        let stripID: EngineBridge.StripID
        var attempts: Int
    }

    /// Max activation attempts before giving up until the next audible signal.
    private let maxAttempts = 5

    /// Low-frequency fallback timer that re-attempts deferred taps. The primary
    /// retry signal is `AppState.reconcileSources` calling `retryPendingTaps()`
    /// whenever AppMonitor's audible set changes; this timer stays as
    /// belt-and-braces for failures the HAL listeners can't see (e.g. a tap
    /// that failed for a transient reason while the audible set was stable).
    private var retryTimer: Timer?

    init(engine: EngineBridge) {
        self.engine = engine
        startRetryTimer()
    }

    deinit { retryTimer?.invalidate() }

    private func startRetryTimer() {
        let timer = Timer(timeInterval: 2.0, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.retryPendingTaps() }
        }
        RunLoop.main.add(timer, forMode: .common)
        retryTimer = timer
    }

    // MARK: Public API

    /// Start (or queue) a tap for `bundleId`, feeding engine strip `stripID`.
    func startTap(forBundleId bundleId: String, stripID: EngineBridge.StripID) {
        guard taps[bundleId] == nil else { return }   // already active
        attemptActivation(bundleId: bundleId, stripID: stripID, priorAttempts: 0)
    }

    /// Stop and destroy the tap for `bundleId`, if any.
    func stopTap(forBundleId bundleId: String) {
        pending[bundleId] = nil
        if let tap = taps.removeValue(forKey: bundleId) {
            tap.tearDown()
        }
    }

    /// Tear down every tap (called on quit).
    func removeAllTaps() {
        for tap in taps.values { tap.tearDown() }
        taps.removeAll()
        pending.removeAll()
    }

    /// Re-attempt any pending taps. Call this when a process transitions to
    /// audible (driven by `AppMonitor`'s HAL listener).
    func retryPendingTaps() {
        for (bundleId, info) in pending {
            attemptActivation(bundleId: bundleId,
                              stripID: info.stripID,
                              priorAttempts: info.attempts)
        }
    }

    // MARK: Activation with defer-until-audible retry

    private func attemptActivation(bundleId: String,
                                   stripID: EngineBridge.StripID,
                                   priorAttempts: Int) {
        let tap = ProcessTap(bundleId: bundleId, stripID: stripID)
        do {
            try tap.activate()
            taps[bundleId] = tap
            pending[bundleId] = nil
        } catch ProcessTapError.processNotFound {
            // Not yet audible — defer. AppMonitor's audible listener (via
            // reconcileSources → retryPendingTaps) and the fallback timer
            // both re-attempt this entry.
            queuePending(bundleId: bundleId, stripID: stripID,
                         attempts: priorAttempts + 1)
        } catch {
            // Other failures: bounded retry, then give up until next audible.
            queuePending(bundleId: bundleId, stripID: stripID,
                         attempts: priorAttempts + 1)
        }
    }

    private func queuePending(bundleId: String,
                              stripID: EngineBridge.StripID,
                              attempts: Int) {
        guard attempts < maxAttempts else {
            pending[bundleId] = nil
            return
        }
        pending[bundleId] = PendingTap(stripID: stripID, attempts: attempts)
    }

    // MARK: Introspection

    var activeBundleIds: [String] { Array(taps.keys) }

    func isTapped(_ bundleId: String) -> Bool { taps[bundleId] != nil }

    /// Most recent peak (linear) captured by the tap for `bundleId`, or 0 if the
    /// app isn't tapped. Consumed by the UI metering loop.
    func takePeak(forBundleId bundleId: String) -> Float {
        taps[bundleId]?.takePeak() ?? 0
    }
}
