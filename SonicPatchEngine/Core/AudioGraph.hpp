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
//   * The previously-live config is pushed onto a retirement queue and freed
//     later by retireOldConfigs(), which the control thread calls periodically
//     (e.g. from a low-priority timer). This guarantees the audio thread never
//     dereferences freed memory: an old config is only freed once we know the
//     audio thread has moved on to a newer one.
//
// This is a classic single-writer (control thread) / single-reader (audio
// thread) RCU-style swap. Only one publisher is assumed; publish() is not
// re-entrant and must be serialized by the caller (the AudioEngine does this).
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

    /// Free configs that have been superseded and are guaranteed no longer in
    /// use by the audio thread. Control thread only, non-RT.
    void retireOldConfigs();

    /// Snapshot the live config generation (diagnostics). Lock-free read.
    uint64_t liveGeneration() const noexcept;

private:
    // The currently-live config. Audio thread reads with acquire; control thread
    // writes with release.
    std::atomic<GraphConfig*> current_{nullptr};

    // Retirement: control thread parks superseded configs here; they are freed
    // by retireOldConfigs(). A generation barrier ensures we never free a config
    // until a *newer* one has been observed live for at least one publish cycle.
    std::vector<std::unique_ptr<GraphConfig>> retired_;

    std::atomic<uint64_t> generationCounter_{0};
};

} // namespace sonicpatch
