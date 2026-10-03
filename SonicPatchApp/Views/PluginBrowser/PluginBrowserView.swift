//
//  PluginBrowserView.swift
//  SonicPatch
//
//  Placeholder for the Audio Unit browser (Phase 4): a searchable list of
//  installed AU components plus the built-in DSP catalogue, used to fill an
//  insert slot on a channel strip.
//

import SwiftUI

struct PluginBrowserView: View {
    /// Strip + slot this browser is inserting into (set by the caller).
    var stripID: EngineBridge.StripID?
    var slotIndex: Int = 0

    @State private var query = ""

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text("Insert Effect").font(.headline)
                Spacer()
                Text("Phase 4 — preview")
                    .font(.caption).foregroundStyle(.tertiary)
            }

            TextField("Search effects…", text: $query)
                .textFieldStyle(.roundedBorder)

            List {
                Section("Built-in DSP") {
                    ForEach(filteredBuiltins) { effect in
                        Button(effect.displayName) {
                            // TODO(Phase 3/4): call EngineBridge.insertBuiltinEffect
                            // for stripID/slotIndex and dismiss.
                        }
                        .buttonStyle(.plain)
                    }
                }
                Section("Audio Units") {
                    // TODO(Phase 4): enumerate AU components via
                    // AVAudioUnitComponentManager and list them here, filtered
                    // by `query`. Insert via EngineBridge / AudioUnitHost.
                    Text("Audio Unit discovery arrives in Phase 4.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
        }
        .padding()
        .frame(minWidth: 320, minHeight: 360)
    }

    private var filteredBuiltins: [BuiltinEffectKind] {
        guard !query.isEmpty else { return BuiltinEffectKind.allCases }
        return BuiltinEffectKind.allCases.filter {
            $0.displayName.localizedCaseInsensitiveContains(query)
        }
    }
}
