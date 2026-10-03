//
//  Session.swift
//  SonicPatch
//
//  Codable session / preset model. A session captures the full per-source mix
//  and routing configuration so it can be re-applied across app launches.
//
//  Persisted as JSON to:
//    ~/Library/Application Support/SonicPatch/Presets/<name>.json
//
//  STATUS: model + persistence are implemented but NOT yet wired to the app —
//  nothing loads or saves sessions until Phase 6 (auto-save / named presets).
//  This file is the canonical encoding of the persistence schema; treat field
//  renames as a persisted-format change.
//

import Foundation

/// A persisted preset: per-source strip configs (keyed by bundle id) plus the
/// routing graph.
struct Session: Codable, Equatable {
    var name: String
    var formatSampleRate: Double
    var sources: [String: StripConfig]   ///< Keyed by bundle identifier.
    var routing: RoutingConfig

    init(name: String = "Default",
         formatSampleRate: Double = 48_000,
         sources: [String: StripConfig] = [:],
         routing: RoutingConfig = RoutingConfig()) {
        self.name = name
        self.formatSampleRate = formatSampleRate
        self.sources = sources
        self.routing = routing
    }
}

/// Per-strip configuration for one source.
struct StripConfig: Codable, Equatable {
    var volumeDB: Float = 0
    var pan: Float = 0
    var muted: Bool = false
    var inputTrimDB: Float = 0
    var outputDeviceUID: String?
    var preEffects: [EffectConfig] = []   ///< Up to 4 pre-fader inserts.
    var postEffects: [EffectConfig] = []  ///< Up to 4 post-fader inserts.
}

/// One effect insert (built-in DSP or hosted AudioUnit) and its parameters.
struct EffectConfig: Codable, Equatable {
    enum Kind: String, Codable {
        case builtin
        case audioUnit
    }
    var kind: Kind
    /// For `.builtin`: the `BuiltinEffectType` raw name. For `.audioUnit`: a
    /// component identifier (type/subtype/manufacturer encoded as a string).
    var identifier: String
    var bypassed: Bool = false
    /// Effect parameters by paramId. AU opaque state is stored separately.
    var parameters: [String: Float] = [:]
}

/// Inter-app routing graph (Phase 5). Empty by default.
struct RoutingConfig: Codable, Equatable {
    /// Edges from a source bundle id to a destination (device UID or bus name).
    var edges: [RoutingEdge] = []
}

struct RoutingEdge: Codable, Equatable {
    var sourceBundleId: String
    var destination: String   ///< Output device UID or virtual bus name.
}

// MARK: - Persistence

extension Session {

    /// Directory where presets are stored, created on demand.
    static var presetsDirectory: URL {
        let base = FileManager.default
            .urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        return base
            .appendingPathComponent("SonicPatch", isDirectory: true)
            .appendingPathComponent("Presets", isDirectory: true)
    }

    /// File URL for a named preset.
    static func url(forName name: String) -> URL {
        presetsDirectory.appendingPathComponent("\(name).json")
    }

    /// Write this session to disk as pretty-printed JSON.
    func save() throws {
        let dir = Self.presetsDirectory
        try FileManager.default.createDirectory(at: dir,
                                                withIntermediateDirectories: true)
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        let data = try encoder.encode(self)
        try data.write(to: Self.url(forName: name), options: .atomic)
        // TODO(Phase 6): debounce writes; keep a "last session" symlink/pointer.
    }

    /// Load a named session from disk.
    static func load(name: String) throws -> Session {
        let data = try Data(contentsOf: url(forName: name))
        return try JSONDecoder().decode(Session.self, from: data)
    }

    /// List the names of all saved presets.
    static func availablePresetNames() -> [String] {
        let urls = (try? FileManager.default.contentsOfDirectory(
            at: presetsDirectory,
            includingPropertiesForKeys: nil)) ?? []
        return urls
            .filter { $0.pathExtension == "json" }
            .map { $0.deletingPathExtension().lastPathComponent }
            .sorted()
    }
}
