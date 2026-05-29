#pragma once
//
// Node.hpp
// Internal base class for every graph node.
//
// IMPORTANT: virtuality lives HERE, in the internal C++ layer, and never leaks
// into the public AudioEngine facade. The audio thread calls process() through
// this vtable; that single indirection is acceptable and is the standard design
// for an audio graph. All concrete nodes pre-allocate their working state in
// prepare() (non-RT) so that process() (RT) allocates nothing.
//
#include "../Core/AudioBuffer.hpp"
#include "../Public/EngineTypes.hpp"

#include <cstdint>

namespace sonicpatch {

/// Everything a node needs for one rendering pass. Passed by reference on the
/// audio thread. Holds only pointers/views into pre-allocated storage.
struct ProcessContext {
    AudioBuffer* inputs    = nullptr;  ///< Array of `numInputs` input views.
    int          numInputs = 0;
    AudioBuffer* outputs   = nullptr;  ///< Array of `numOutputs` output views.
    int          numOutputs = 0;
    uint32_t     frames     = 0;       ///< Frames to process this pass (<= block).
    double       sampleRate = 48000.0;
};

/// Abstract internal node. NOT exposed publicly.
class Node {
public:
    explicit Node(NodeKind kind, NodeID id) noexcept : kind_(kind), id_(id) {}
    virtual ~Node() = default;

    Node(const Node&)            = delete;
    Node& operator=(const Node&) = delete;

    NodeKind kind() const noexcept { return kind_; }
    NodeID   id()   const noexcept { return id_; }

    /// Non-RT: allocate working state for the given format. Called whenever the
    /// format changes, before the node is published into a live config.
    virtual void prepare(const AudioFormat& fmt) = 0;

    /// Non-RT: clear any internal state (e.g. filter histories, ramps) without
    /// reallocating. Safe to call before re-starting the engine.
    virtual void reset() {}

    /// [RT] The hot path. Read inputs, write outputs. Must not allocate, lock,
    /// block, or throw. See RtSafe.hpp for the full rule list.
    virtual void process(ProcessContext& ctx) = 0;

protected:
    NodeKind kind_;
    NodeID   id_;
};

} // namespace sonicpatch
