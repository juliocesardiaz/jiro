//
//  MenuBarView.swift
//  SonicPatch
//
//  The menu-bar popover: a compact mixer listing every audio-producing app with
//  a volume slider, mute toggle, output-device picker, FX indicator, and a peak
//  level meter.
//

import SwiftUI

struct MenuBarView: View {
    @EnvironmentObject private var appState: AppState
    @Environment(\.openWindow) private var openWindow

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            header

            Divider()

            if appState.sources.isEmpty {
                emptyState
            } else {
                ScrollView {
                    LazyVStack(spacing: 8) {
                        ForEach(appState.sources) { source in
                            SourceRow(source: source)
                                .padding(.horizontal, 12)
                        }
                    }
                    .padding(.vertical, 8)
                }
                .frame(maxHeight: 360)
            }

            Divider()

            footer
        }
        .frame(width: 340)
    }

    // MARK: Sections

    private var header: some View {
        HStack {
            Image(systemName: "slider.horizontal.3")
            Text("SonicPatch")
                .font(.headline)
            Spacer()
            Circle()
                .fill(appState.isRunning ? Color.green : Color.secondary)
                .frame(width: 8, height: 8)
                .help(appState.isRunning ? "Engine running" : "Engine stopped")
        }
        .padding(12)
    }

    private var emptyState: some View {
        VStack(spacing: 6) {
            Image(systemName: "speaker.slash")
                .font(.largeTitle)
                .foregroundStyle(.secondary)
            Text("No audio-producing apps")
                .foregroundStyle(.secondary)
            Text("Play audio in an app to see it here.")
                .font(.caption)
                .foregroundStyle(.tertiary)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 28)
    }

    private var footer: some View {
        HStack {
            Button("Open Mixer") { openWindow(id: "mixer") }
            Spacer()
            Button("Quit") {
                appState.stop()
                NSApplication.shared.terminate(nil)
            }
        }
        .padding(12)
    }
}

/// A single app's row: icon, name, FX badge, output picker, mute, volume + meter.
private struct SourceRow: View {
    @EnvironmentObject private var appState: AppState
    let source: AudioSource

    /// Live level for this source's strip, if any.
    private var level: LevelReading {
        guard let strip = source.stripID else { return LevelReading() }
        return appState.levels[strip] ?? LevelReading()
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 8) {
                iconView
                Text(source.displayName)
                    .lineLimit(1)
                    .font(.subheadline)
                if source.hasEffects {
                    Text("FX")
                        .font(.caption2.bold())
                        .padding(.horizontal, 5).padding(.vertical, 1)
                        .background(Capsule().fill(Color.accentColor.opacity(0.2)))
                }
                Spacer()
                OutputDevicePicker(source: source)
            }

            HStack(spacing: 8) {
                Button {
                    appState.setMute(!source.muted, for: source)
                } label: {
                    Image(systemName: source.muted
                          ? "speaker.slash.fill" : "speaker.wave.2.fill")
                }
                .buttonStyle(.borderless)
                .help(source.muted ? "Unmute" : "Mute")

                Slider(
                    value: Binding(
                        get: { Double(source.volumeDB) },
                        set: { appState.setVolume(Float($0), for: source) }),
                    in: -60...6)
                .disabled(source.muted)

                Text(volumeLabel)
                    .font(.caption.monospacedDigit())
                    .frame(width: 44, alignment: .trailing)
                    .foregroundStyle(.secondary)
            }

            PeakMeter(level: level)
                .frame(height: 4)
        }
        .padding(8)
        .background(RoundedRectangle(cornerRadius: 8).fill(Color.primary.opacity(0.04)))
    }

    @ViewBuilder private var iconView: some View {
        if let icon = source.icon {
            Image(nsImage: icon)
                .resizable()
                .frame(width: 18, height: 18)
        } else {
            Image(systemName: "app.dashed")
                .frame(width: 18, height: 18)
        }
    }

    private var volumeLabel: String {
        source.volumeDB <= -60 ? "-∞" : String(format: "%+.0f", source.volumeDB)
    }
}

/// Per-source output-device picker. DISABLED until Phase 2: audio currently
/// always follows the system default output, and an enabled picker bound to
/// mock devices would silently discard the user's choice.
private struct OutputDevicePicker: View {
    @EnvironmentObject private var appState: AppState
    let source: AudioSource

    // TODO(Phase 2): populate from Core Audio output-device enumeration and
    // re-enable; route the selection through AppState -> ProcessTap.
    var body: some View {
        Picker("", selection: .constant("System Default")) {
            Text("System Default").tag("System Default")
        }
        .labelsHidden()
        .frame(maxWidth: 130)
        .font(.caption)
        .disabled(true)
        .help("Per-source output selection arrives in Phase 2; audio follows the system default output.")
    }
}

/// A simple horizontal peak meter, 0 at -60 dBFS to full at 0 dBFS.
private struct PeakMeter: View {
    let level: LevelReading

    private var fraction: CGFloat {
        let db = level.peakDB
        let clamped = max(-60, min(0, db))
        return CGFloat((clamped + 60) / 60)
    }

    var body: some View {
        GeometryReader { geo in
            ZStack(alignment: .leading) {
                Capsule().fill(Color.primary.opacity(0.1))
                Capsule()
                    .fill(meterColor)
                    .frame(width: geo.size.width * fraction)
            }
        }
    }

    private var meterColor: Color {
        switch level.peakDB {
        case ..<(-6): return .green
        case ..<(-1): return .yellow
        default:      return .red
        }
    }
}
