#pragma once
//
// TapSourceNode.hpp
// Graph source that pulls audio captured by a Core Audio process Tap.
//
// The Core Audio Tap IOProc (Phase 1, macOS 14.4+ CATap / AudioHardwareTap API)
// runs on its own real-time thread and writes captured frames into a
// pre-allocated, lock-free scratch staging area. This node, running on the
// engine's render thread, drains that staging area into the graph buffer.
//
// On non-Apple builds (and until the Tap is wired up) process() emits silence so
// the rest of the graph is exercisable. The Core Audio parts are guarded by
// __APPLE__ and stubbed with the correct call sites noted in comments.
//
#include "Node.hpp"

#include <atomic>
#include <string>
#include <vector>

namespace sonicpatch {

class TapSourceNode final : public Node {
public:
    TapSourceNode(NodeID id, const char* bundleId);

    void prepare(const AudioFormat& fmt) override;
    void reset() override;
    void process(ProcessContext& ctx) override; // [RT]

    /// Called from the Tap IOProc thread to deposit captured audio. [RT]
    /// Copies up to `frames` of `channels` into the staging buffer.
    void depositFromTap(const float* const* src, uint32_t channels, uint32_t frames) noexcept;

    const char* bundleId() const noexcept { return bundleId_.c_str(); }

private:
    std::string bundleId_;
    AudioFormat format_;

    // Pre-allocated staging storage filled by the Tap IOProc and drained here.
    // For Phase 1 a simple double-buffer / SPSC ring would replace this; kept as
    // a flat scratch buffer with an atomic "frames available" for the skeleton.
    std::vector<std::vector<float>> staging_;       ///< [channel][frame]
    std::atomic<uint32_t>           stagedFrames_{0};

    // TODO(Phase 1): hold the AudioHardwareTap / aggregate-device handles here:
    //   AudioObjectID tapId_; AudioDeviceID aggregateId_; AudioDeviceIOProcID proc_;
    // Guarded by __APPLE__ in the .cpp; created in start(), destroyed in stop().
};

} // namespace sonicpatch
