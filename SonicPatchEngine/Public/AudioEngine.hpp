#pragma once
//
// AudioEngine.hpp
// Public, NON-VIRTUAL facade for the SonicPatch audio engine.
//
// Design notes:
//  * pImpl idiom: all internal C++ types (graph, nodes, DSP, Core Audio) are
//    hidden behind an opaque Impl pointer. This keeps this header free of any
//    Apple framework includes so it imports cleanly into Swift via C++ interop.
//  * Every method is non-virtual. Swift C++ interop dislikes virtual public
//    APIs; the virtuality lives entirely in the internal Node layer.
//  * Keep this header light: only EngineTypes.hpp (fixed-width PODs) is needed.
//
#include "EngineTypes.hpp"

#include <memory>

// Forward declaration for Phase-4 AudioUnit hosting. We deliberately do NOT
// include <AudioToolbox/AudioToolbox.h> here so that this header stays portable
// and Swift-interop-friendly. On Apple builds the real type is visible; on other
// platforms we provide a tag so the signature still compiles.
#if defined(__APPLE__)
struct AudioComponentDescription; // from <AudioToolbox/AUComponent.h>
#else
struct AudioComponentDescription;  // opaque tag on non-Apple builds (Phase 4)
#endif

namespace sonicpatch {

/// The single entry point used by the Swift UI layer. Thread-safety contract:
///  * Mutating methods (create/remove/setX, insert/removeEffect) are called from
///    the main/UI thread and are serialized internally; they publish a new
///    immutable graph config to the audio thread via an atomic swap.
///  * getLevel() is lock-free: it resolves the strip through an atomically-
///    published snapshot map and reads meter atomics. Safe to poll at 60 fps
///    without contending with control operations.
///
/// Handle semantics: AudioEngine is a *copyable shared handle* — the internal
/// state lives behind a shared Impl, and copies refer to the same engine. This
/// is deliberate for Swift C++ interop: Swift 5.9 cannot import move-only C++
/// types, so the facade must be copyable to be usable as a Swift stored
/// property. Copying never duplicates the audio graph.
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    // Copyable shared handle (see above); copies alias the same engine.
    AudioEngine(const AudioEngine&)            = default;
    AudioEngine& operator=(const AudioEngine&) = default;
    AudioEngine(AudioEngine&&) noexcept;
    AudioEngine& operator=(AudioEngine&&) noexcept;

    // --- Lifecycle ---------------------------------------------------------
    void start();
    void stop();
    bool isRunning() const;

    /// Reconfigure rate/geometry. Must be called while stopped (re-allocates
    /// the buffer pool and rebuilds the graph config). Not RT-safe.
    void setAudioFormat(AudioFormat fmt);

    // --- Channel strips ----------------------------------------------------
    /// Create a strip that captures the process identified by `bundleId`
    /// (e.g. "com.spotify.client"). Returns kInvalidStrip on failure.
    StripID createChannelStrip(const char* bundleId);
    void    removeChannelStrip(StripID strip);

    // --- Per-strip controls (UI thread; smoothed on the audio thread) ------
    void setVolume(StripID strip, float db);
    void setPan(StripID strip, float pan);   ///< -1 (L) .. 0 (C) .. +1 (R)
    void setMute(StripID strip, bool muted);
    void setInputTrim(StripID strip, float db);

    /// Lock-free metering read. Returns {0,0} for unknown strips.
    LevelSnapshot getLevel(StripID strip) const;

    // --- Effects -----------------------------------------------------------
    /// Insert a builtin DSP effect into a strip's effect rack at `slotIndex`.
    /// Returns an opaque slot handle (>= 0) or -1 on failure.
    int  insertBuiltinEffect(StripID strip, int slotIndex, BuiltinEffectType type);

    /// Set an effect parameter. `paramId` is effect-specific (see DSP headers).
    void setEffectParameter(StripID strip, int slot, uint32_t paramId, float value);

    /// Remove an effect slot.
    void removeEffect(StripID strip, int slot);

    // --- AudioUnit hosting (Phase 4 — STUBBED) -----------------------------
    /// Insert a hosted third-party AudioUnit described by `desc`. Declared here
    /// for API stability but currently a stub (returns -1). The real
    /// implementation lives behind AU/AudioUnitHost and is gated on __APPLE__.
    /// @see AU/AudioUnitHost.h
    int insertEffect(StripID strip, int slotIndex, const AudioComponentDescription& desc);

private:
    struct Impl;                  ///< Opaque; defined in AudioEngine.cpp.
    std::shared_ptr<Impl> impl_;  ///< Shared so the facade is copyable (Swift).
};

} // namespace sonicpatch
