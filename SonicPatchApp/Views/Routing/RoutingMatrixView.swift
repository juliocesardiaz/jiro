//
//  RoutingMatrixView.swift
//  SonicPatch
//
//  Placeholder for the inter-app routing matrix (Phase 5): sources down the
//  side, buses/destinations across the top, with toggleable cross-points.
//

import SwiftUI

struct RoutingMatrixView: View {
    @EnvironmentObject private var appState: AppState

    // Mock destinations until the HAL virtual buses + device enumeration land.
    private let destinations = ["Speakers", "Headphones", "Bus 1", "Bus 2"]

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text("Routing Matrix").font(.headline)
                Spacer()
                Text("Phase 5 — preview")
                    .font(.caption).foregroundStyle(.tertiary)
            }

            if appState.sources.isEmpty {
                ContentUnavailableView("No sources to route",
                                       systemImage: "arrow.triangle.branch")
            } else {
                matrix
            }
        }
        .padding()
    }

    private var matrix: some View {
        Grid(alignment: .center, horizontalSpacing: 12, verticalSpacing: 8) {
            // Column headers
            GridRow {
                Text("").gridColumnAlignment(.leading)
                ForEach(destinations, id: \.self) { dest in
                    Text(dest).font(.caption).rotationEffect(.degrees(0))
                }
            }
            Divider()
            // One row per source
            ForEach(appState.sources) { source in
                GridRow {
                    Text(source.displayName)
                        .font(.caption)
                        .lineLimit(1)
                        .gridColumnAlignment(.leading)
                    ForEach(destinations, id: \.self) { _ in
                        CrossPoint()
                    }
                }
            }
        }
    }
}

/// A single matrix cross-point toggle (mock; not yet connected to the engine).
private struct CrossPoint: View {
    @State private var on = false
    var body: some View {
        Button {
            on.toggle()
            // TODO(Phase 5): apply RoutingEdge and reconfigure the engine graph.
        } label: {
            Image(systemName: on ? "circle.fill" : "circle")
                .foregroundStyle(on ? Color.accentColor : Color.secondary)
        }
        .buttonStyle(.plain)
    }
}
