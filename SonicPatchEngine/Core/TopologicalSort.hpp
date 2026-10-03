#pragma once
//
// TopologicalSort.hpp
// Kahn's-algorithm topological sort over the audio graph DAG.
//
// Edges are directed producer -> consumer. The sorted order guarantees that
// when the audio thread walks nodes front-to-back, every node's inputs have
// already been produced this block. Cycles are illegal (they imply an infinite
// feedback loop with no delay) and are reported as failure.
//
#include "../Public/EngineTypes.hpp"

#include <cstdint>
#include <vector>

namespace sonicpatch {

/// A directed edge from `from` (producer) to `to` (consumer).
struct Edge {
    NodeID from;
    NodeID to;
};

struct TopoSortResult {
    bool                 ok = false;  ///< false if a cycle was detected.
    std::vector<NodeID>  order;       ///< valid only when ok == true.
};

/// Run Kahn's algorithm. `nodes` is the full set of node ids; `edges` the
/// dependency edges. If a cycle exists, returns {false, {}}. This runs at
/// config-build time on a non-RT thread.
TopoSortResult topologicalSort(const std::vector<NodeID>& nodes,
                               const std::vector<Edge>& edges);

} // namespace sonicpatch
