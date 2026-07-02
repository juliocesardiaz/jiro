#pragma once
//
// AudioGraph.hpp
// Owns the live graph and performs lock-free config hot-swapping.
//
// Concurrency model:
//   * The control (UI/main) thread builds a new GraphConfig and calls publish().
//     publish() does a release-store of the new config pointer.
//   * The audio thread, inside process(), does an acquire-load of the current
//     config pointer and walks it. It never frees anything.
//   * The previously-live config is pushed onto a retirement queue, stamped
//     with the render epoch observed at retirement time. retireOldConfigs()
//     frees a retired config only after the audio thread has advanced the
//     epoch by at least two callbacks past that stamp — proof that any
//     callback which could have been inside the old config has completed.
//   * The audio thread increments `renderEpoch_` at the top of every
//     process() call. If no audio thread is running, the epoch never advances
//     and retired configs are held; call drain() (only when provably no RT
//     thread is active, e.g. after stop()) to reclaim them.
//
// This is a classic single-writer (control thread) / single-reader (audio
// thread) RCU-style swap with epoch-based reclamation. Only one publisher is
// assumed; publish() is not re-entrant and must be serialized by the caller
// (the AudioEngine does this).
//
#include "GraphConfig.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace sonicpatch {

class AudioGraph {
public:
    AudioGraph();
    ~AudioGraph();

    AudioGraph(const AudioGraph&)            = delete;
    AudioGraph& operator=(const AudioGraph&) = delete;

    /// Publish a new immutable config (control thread). Takes ownership. The old
    /// config is moved to the retirement queue and freed later by
    /// retireOldConfigs(). Non-RT.
    void publish(std::unique_ptr<GraphConfig> next);

    /// [RT] Render one block by walking the currently-published config.
    /// Acquire-loads the config pointer; if null, does nothing. Allocates
    /// nothing, takes no locks.
    void process(const AudioFormat& fmt, uint32_t frames) noexcept;

    /// Free retired configs whose retirement epoch the audio thread has since
    /// moved at least two callbacks past. Safe to call at any time from the
    /// control thread; configs the audio thread could still be inside are kept.
    void retireOldConfigs();

    /// Free ALL retired configs unconditionally. Only call when no audio
    /// thread can be inside process() — e.g. after the IOProcs are stopped, or
    /// when the engine never started. Control thread only.
    void drain();

    /// Snapshot the live config generation (diagnostics). Lock-free read.
    uint64_t liveGeneration() const noexcept;

    /// Number of configs awaiting reclamation (diagnostics/tests).
    std::size_t retiredCount() const noexcept { return retired_.size(); }

private:
    // The currently-live config. Audio thread reads with acquire; control thread
    // writes with release.
    std::atomic<GraphConfig*> current_{nullptr};

    // Incremented by the audio thread at the top of every process() call.
    // Control thread reads it (acquire) to prove a retired config is unreachable.
    std::atomic<uint64_t> renderEpoch_{0};

    // Retirement queue: superseded configs stamped with the render epoch at
    // retirement time. Freed by retireOldConfigs() once the epoch has advanced
    // past the stamp by >= 2 (two callback starts imply the callback that could
    // have held the config has completed), or unconditionally by drain().
    struct RetiredConfig {
        std::unique_ptr<GraphConfig> config;
        uint64_t                     epochAtRetire = 0;
    };
    std::vector<RetiredConfig> retired_;

    std::atomic<uint64_t> generationCounter_{0};
};

} // namespace sonicpatch
