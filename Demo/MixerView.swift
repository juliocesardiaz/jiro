import SwiftUI
import AppKit

private let accent = Color(red: 0.37, green: 0.84, blue: 0.72)

struct MixerView: View {
    @EnvironmentObject private var model: DemoModel
    @Environment(\.openWindow) private var openWindow
    let compact: Bool

    var body: some View {
        VStack(spacing: 0) {
            VStack(alignment: .leading, spacing: 18) {
                HStack(spacing: 11) {
                    Image(systemName: "waveform")
                        .font(.system(size: 25, weight: .medium))
                        .foregroundStyle(accent)
                        .frame(width: 44, height: 44)
                        .background(accent.opacity(0.12), in: RoundedRectangle(cornerRadius: 13))
                    VStack(alignment: .leading, spacing: 3) {
                        Text("SonicPatch").font(.system(size: 23, weight: .semibold, design: .rounded))
                        Text("Your apps. Your mix.").font(.subheadline).foregroundStyle(.secondary)
                    }
                    Spacer()
                    Text("DEMO").font(.system(size: 9, weight: .bold, design: .monospaced))
                        .tracking(1.2).padding(.horizontal, 9).padding(.vertical, 5)
                        .background(.white.opacity(0.07), in: Capsule()).foregroundStyle(.secondary)
                }
                HStack(spacing: 10) {
                    Image(systemName: "speaker.wave.2").foregroundStyle(accent)
                    VStack(alignment: .leading, spacing: 2) {
                        Text("SYSTEM OUTPUT").font(.system(size: 9, weight: .semibold)).tracking(1.1).foregroundStyle(.secondary)
                        Text(model.defaultOutputName).font(.system(size: 13, weight: .medium)).lineLimit(1)
                    }
                    Spacer()
                    Circle().fill(model.enabled ? accent : .gray).frame(width: 6, height: 6)
                    Text(model.enabled ? "Mixing" : "Bypassed").font(.caption).foregroundStyle(.secondary)
                }
                .padding(13)
                .background(.white.opacity(0.04), in: RoundedRectangle(cornerRadius: 12))
            }
            .padding(compact ? 20 : 26)

            if !model.enabled {
                VStack(alignment: .leading, spacing: 12) {
                    Text("Give every app its own volume.").font(.headline)
                    Text("Start mixing, then allow macOS to capture system audio. Audio stays on this Mac and is never recorded or uploaded.")
                        .font(.system(size: 12)).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                    Button(action: model.start) {
                        Label("Start mixing", systemImage: "play.fill")
                            .font(.system(size: 13, weight: .semibold)).frame(maxWidth: .infinity).padding(.vertical, 7)
                    }
                    .buttonStyle(.borderedProminent).tint(accent).foregroundStyle(.black)
                }
                .padding(16)
                .background(accent.opacity(0.06), in: RoundedRectangle(cornerRadius: 14))
                .padding(.horizontal, compact ? 20 : 26).padding(.bottom, 18)
            }

            HStack {
                Text("APPLICATIONS").font(.system(size: 10, weight: .semibold)).tracking(1.4)
                Spacer()
                Text("\(model.active.count) controlled").font(.caption)
            }
            .foregroundStyle(.secondary).padding(.horizontal, compact ? 22 : 28).padding(.bottom, 10)

            ScrollView {
                LazyVStack(spacing: 10) {
                    if model.apps.isEmpty {
                        VStack(spacing: 12) {
                            Image(systemName: "music.note").font(.system(size: 30)).foregroundStyle(accent.opacity(0.7))
                            Text("Play something.").font(.headline)
                            Text("Open Music, Spotify, or a video.\nApps appear here when they play audio.")
                                .font(.subheadline).foregroundStyle(.secondary).multilineTextAlignment(.center)
                        }.frame(maxWidth: .infinity).padding(.vertical, 42)
                    }
                    ForEach(model.apps) { app in
                        AppRow(app: app).environmentObject(model)
                    }
                }
                .padding(.horizontal, compact ? 20 : 26).padding(.bottom, 16)
            }

            Divider().opacity(0.35)
            HStack(spacing: 14) {
                if model.enabled {
                    Button("Restore audio", systemImage: "arrow.uturn.backward") { model.stop() }
                        .help("Release all captured apps and return to their normal audio playback.")
                } else {
                    Text("Native audio · Local only").font(.caption).foregroundStyle(.secondary)
                }
                Spacer()
                Menu {
                    if compact {
                        Button("Open mixer window") {
                            openWindow(id: "mixer")
                            NSApp.activate(ignoringOtherApps: true)
                        }
                    }
                    Button("Audio recording permission…", action: model.openAudioPrivacy)
                    Button("Refresh applications", action: model.refresh)
                    Divider()
                    Button("Quit SonicPatch") { model.stop(); NSApp.terminate(nil) }
                        .keyboardShortcut("q")
                } label: { Image(systemName: "ellipsis.circle").font(.system(size: 17)) }
                .menuStyle(.borderlessButton).fixedSize()
            }
            .buttonStyle(.plain).font(.system(size: 12)).padding(18)
        }
        .background(Color(red: 0.065, green: 0.08, blue: 0.09))
        .preferredColorScheme(.dark).tint(accent)
    }
}

private struct AppRow: View {
    @EnvironmentObject private var model: DemoModel
    let app: PlayingApp
    @State private var expanded = false
    private var mix: AppMix { model.mix(app.id) }
    private var controlled: Bool { model.active.contains(app.id) }

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(spacing: 10) {
                Group {
                    if let icon = app.icon { Image(nsImage: icon).resizable() }
                    else { Image(systemName: "app.fill").resizable().foregroundStyle(.secondary) }
                }.frame(width: 29, height: 29)
                VStack(alignment: .leading, spacing: 2) {
                    Text(app.name).font(.system(size: 13, weight: .semibold)).lineLimit(1)
                    Text(controlled ? (app.isPlaying ? "Controlled" : "Ready") : (model.enabled ? "Waiting" : "Normal playback"))
                        .font(.system(size: 10)).foregroundStyle(.secondary)
                }
                Spacer()
                Button {
                    model.change(app.id) { $0.muted.toggle() }
                } label: {
                    Image(systemName: mix.muted ? "speaker.slash.fill" : "speaker.wave.2.fill")
                        .foregroundStyle(mix.muted ? .orange : accent)
                        .frame(width: 29, height: 29)
                        .background(.white.opacity(0.04), in: RoundedRectangle(cornerRadius: 8))
                }
                .buttonStyle(.plain).disabled(!controlled)
                .accessibilityLabel(mix.muted ? "Unmute \(app.name)" : "Mute \(app.name)")
                Button { expanded.toggle() } label: {
                    Image(systemName: expanded ? "chevron.up" : "chevron.down")
                        .font(.system(size: 10, weight: .semibold)).frame(width: 22, height: 28)
                }.buttonStyle(.plain).accessibilityLabel("Balance and output for \(app.name)")
            }
            HStack(spacing: 14) {
                Slider(value: Binding(get: { mix.volume }, set: { value in
                    model.change(app.id) { $0.volume = value }
                }), in: 0...1)
                .disabled(!controlled).accessibilityLabel("\(app.name) volume")
                Text("\(Int((mix.volume * 100).rounded()))%")
                    .font(.system(size: 12, weight: .medium, design: .monospaced))
                    .foregroundStyle(mix.muted ? .secondary : .primary).frame(width: 39, alignment: .trailing)
            }
            MeterView(level: model.levels[app.id] ?? 0).frame(height: 3)
            if expanded {
                HStack(spacing: 10) {
                    Text("L").font(.caption).foregroundStyle(.secondary)
                    Slider(value: Binding(get: { mix.balance }, set: { value in
                        model.change(app.id) { $0.balance = value }
                    }), in: -1...1).disabled(!controlled).accessibilityLabel("\(app.name) balance")
                    Text("R").font(.caption).foregroundStyle(.secondary)
                    Button("Center") { model.change(app.id) { $0.balance = 0 } }
                        .font(.caption).disabled(!controlled)
                }
                Picker("Output", selection: Binding(get: { mix.outputUID }, set: { value in
                    model.change(app.id) { $0.outputUID = value }
                })) {
                    Text("System default").tag("")
                    if !mix.outputUID.isEmpty && !model.outputs.contains(where: { $0.uid == mix.outputUID }) {
                        Text("Disconnected output").tag(mix.outputUID)
                    }
                    ForEach(model.outputs) { output in Text(output.name).tag(output.uid) }
                }
                .font(.caption).accessibilityLabel("Output for \(app.name)")
                Text(app.bundleID).font(.system(size: 9, design: .monospaced))
                    .foregroundStyle(.tertiary).textSelection(.enabled)
            }
            if let error = model.errors[app.id] {
                VStack(alignment: .leading, spacing: 8) {
                    Text(error).font(.caption).foregroundStyle(.orange).fixedSize(horizontal: false, vertical: true)
                    HStack {
                        Button("Retry") { model.retry(app.id) }
                        Button("Permissions…", action: model.openAudioPrivacy)
                    }.font(.caption).buttonStyle(.bordered)
                }
            }
        }
        .padding(15)
        .background(.white.opacity(0.045), in: RoundedRectangle(cornerRadius: 14))
        .overlay(RoundedRectangle(cornerRadius: 14).stroke(.white.opacity(0.05), lineWidth: 1))
    }
}

private struct MeterView: View {
    let level: Float
    private var fraction: CGFloat {
        guard level > 0.00001 else { return 0 }
        return CGFloat(min(1, max(0, (20 * log10(level) + 60) / 60)))
    }
    var body: some View {
        GeometryReader { geometry in
            Capsule().fill(.white.opacity(0.06))
                .overlay(alignment: .leading) {
                    Capsule().fill(level >= 0.98 ? Color.orange : accent)
                        .frame(width: geometry.size.width * fraction)
                }
        }
        .accessibilityLabel("Output level").accessibilityValue("\(Int(fraction * 100)) percent")
    }
}
