#pragma once
//
// GraphConfig.hpp
// An immutable, topologically-sorted snapshot of the audio graph.
//
// A GraphConfig is built on a non-RT (control) thread by the AudioGraph and then
// *published* to the audio thread via an atomic pointer swap (see AudioGraph).
// Once published it is treated as immutable: the audio thread only reads it. The
// audio thread never mutates it and never frees it — retirement of an old config
// happens on a separate non-RT thread.
//
// The config records, for each node (in execution order):
//   * the Node* to invoke,
//   * which pooled buffers feed its inputs,
//   * which pooled buffer(s) receive its outputs.
// The audio thread walks `order` front-to-back; because the order is a valid
// topological sort, every input buffer is already filled by the time a node runs.
//
#include "../Public/EngineTypes.hpp"
#include "AudioBuffer.hpp"

#include <cstdint>
#include <vector>

namespace sonicpatch {

class Node;

/// Binds one node to its I/O buffer views for a single rendering pass. The
/// AudioBuffer views point into BufferPool-owned storage; the config does not
/// own the sample memory.
struct NodeBinding {
    Node*                    node = nullptr;  ///< Not owned (owned by AudioGraph).
    std::vector<AudioBuffer> inputs;          ///< Input buffer views (may be empty).
    std::vector<AudioBuffer> outputs;         ///< Output buffer views.
};

/// Immutable snapshot consumed by the audio thread. Built once per topology
/// change; never mutated after publish().
struct GraphConfig {
    AudioFormat               format;  ///< Rate/geometry this config was built for.
    std::vector<NodeBinding>  order;   ///< Nodes in topological execution order.

    /// Monotonic generation counter — purely for diagnostics / retirement
    /// bookkeeping. Not read on the audio thread for control flow.
    uint64_t generation = 0;
};

} // namespace sonicpatch
