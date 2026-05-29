//
//  ChannelStripView.swift
//  SonicPatch
//
//  Per-source channel strip: input trim, 4 pre-FX insert slots, volume/pan, 4
//  post-FX insert slots, and a meter. This lays out the Phase 3 design; controls
//  are present but operate on mock/local state until the engine effect rack and
//  DSP are wired up.
//

import SwiftUI

struct ChannelStripView: View {
    /// The source this strip represents, if any is selected.
    let source: AudioSource?

    // Local UI state for the layout demo. Real values come from the engine /
    // session config in Phase 3.
    @State private var inputTrimDB: Double = 0
    @State private var volumeDB: Double = 0
    @State private var pan: Double = 0

    var body: some View {
        if let source {
            content(for: source)
        } else {
            ContentUnavailableView("No source selected",
                                   systemImage: "rectangle.dashed",
                                   description: Text("Pick an app to edit its channel strip."))
        }
    }

    private func content(for source: AudioSource) -> some View {
        VStack(spacing: 12) {
            header(for: source)

            HStack(alignment: .top, spacing: 16) {
                // Input trim
                LabeledKnob(title: "Trim", valueDB: $inputTrimDB, range: -24...24)
                    .disabled(true) // TODO(Phase 3): wire to engine.setInputTrim

                // Pre-FX rack
                EffectRack(title: "Pre-FX", slotCount: 4)

                // Fader + pan
                VStack(spacing: 8) {
                    Text("Volume").font(.caption).foregroundStyle(.secondary)
                    Slider(value: $volumeDB, in: -60...6)
                        .frame(height: 160)
                        // Vertical fader.
                        .rotationEffect(.degrees(-90))
                        .frame(width: 160)
                        .disabled(true) // TODO(Phase 3): wire to engine.setVolume
                    Text(volumeDB <= -60 ? "-∞ dB"
                         : String(format: "%+.1f dB", volumeDB))
                        .font(.caption.monospacedDigit())
                    Divider()
                    Text("Pan").font(.caption).foregroundStyle(.secondary)
                    Slider(value: $pan, in: -1...1)
                        .frame(width: 120)
                        .disabled(true) // TODO(Phase 3): wire to engine.setPan
                }

                // Post-FX rack
                EffectRack(title: "Post-FX", slotCount: 4)

                // Meter
                StripMeter()
            }
            Spacer()
        }
        .padding()
        .onAppear {
            inputTrimDB = Double(source.inputTrimDB)
            volumeDB = Double(source.volumeDB)
            pan = Double(source.pan)
        }
    }

    private func header(for source: AudioSource) -> some View {
        HStack {
            if let icon = source.icon {
                Image(nsImage: icon).resizable().frame(width: 22, height: 22)
            }
            Text(source.displayName).font(.headline)
            Spacer()
            Text("Phase 3 — controls disabled")
                .font(.caption).foregroundStyle(.tertiary)
        }
    }
}

/// A vertical rack of insert slots (built-in DSP or hosted AU).
private struct EffectRack: View {
    let title: String
    let slotCount: Int

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(title).font(.caption).foregroundStyle(.secondary)
            ForEach(0..<slotCount, id: \.self) { index in
                EffectSlot(index: index)
            }
        }
        .frame(width: 150)
    }
}

/// One insert slot. Empty by default; tapping would open the plugin browser.
private struct EffectSlot: View {
    let index: Int

    var body: some View {
        Button {
            // TODO(Phase 4): present PluginBrowserView / built-in DSP menu for
            // this slot and call engine.insertBuiltinEffect / insertEffect.
        } label: {
            HStack {
                Image(systemName: "plus.circle")
                Text("Empty")
                Spacer()
            }
            .font(.caption)
            .padding(6)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(RoundedRectangle(cornerRadius: 6)
                .stroke(style: StrokeStyle(lineWidth: 1, dash: [3])))
        }
        .buttonStyle(.plain)
        .foregroundStyle(.secondary)
    }
}

/// A labelled rotary-style control rendered as a compact slider for the stub.
private struct LabeledKnob: View {
    let title: String
    @Binding var valueDB: Double
    let range: ClosedRange<Double>

    var body: some View {
        VStack(spacing: 6) {
            Text(title).font(.caption).foregroundStyle(.secondary)
            Slider(value: $valueDB, in: range)
                .frame(width: 80)
            Text(String(format: "%+.1f dB", valueDB))
                .font(.caption2.monospacedDigit())
        }
        .frame(width: 90)
    }
}

/// Vertical strip meter placeholder.
private struct StripMeter: View {
    var body: some View {
        VStack {
            Text("Meter").font(.caption).foregroundStyle(.secondary)
            RoundedRectangle(cornerRadius: 3)
                .fill(Color.primary.opacity(0.1))
                .frame(width: 12, height: 180)
            // TODO(Phase 3): drive from AppState.levels[strip].
        }
        .frame(width: 60)
    }
}
