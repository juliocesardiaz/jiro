#pragma once
//
// DeviceSinkNode.hpp
// Graph sink that writes the final mix to an output device IOProc.
//
// The output device IOProc (Core Audio) runs on a real-time thread and pulls
// audio from a pre-allocated staging area that this node fills during the graph
// render. On non-Apple builds the device parts are stubbed; the node still
// copies its input into the staging buffer so the pipeline is testable.
//
#include "Node.hpp"

#include <atomic>
#include <vector>

namespace sonicpatch {

class DeviceSinkNode final : public Node {
public:
    explicit DeviceSinkNode(NodeID id) noexcept : Node(NodeKind::DeviceSink, id) {}

    void prepare(const AudioFormat& fmt) override;
    void reset() override;
    void process(ProcessContext& ctx) override; // [RT]

    /// Called from the output device IOProc to fetch the latest rendered block.
    /// [RT] Copies up to `frames` of `channels` out of the staging buffer.
    void fetchForDevice(float* const* dst, uint32_t channels, uint32_t frames) noexcept;

private:
    AudioFormat                     format_;
    std::vector<std::vector<float>> staging_;     ///< [channel][frame]
    std::atomic<uint32_t>           readyFrames_{0};

    // TODO(Phase 1): hold the output AudioDeviceID + AudioDeviceIOProcID here,
    // guarded by __APPLE__; created on start(), torn down on stop().
};

} // namespace sonicpatch
