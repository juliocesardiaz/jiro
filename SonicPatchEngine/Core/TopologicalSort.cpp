//
// TopologicalSort.cpp
//
#include "TopologicalSort.hpp"

#include <queue>
#include <unordered_map>

namespace sonicpatch {

TopoSortResult topologicalSort(const std::vector<NodeID>& nodes,
                               const std::vector<Edge>& edges) {
    TopoSortResult result;

    // Build adjacency list + in-degree count.
    std::unordered_map<NodeID, std::vector<NodeID>> adj;
    std::unordered_map<NodeID, int> indeg;
    adj.reserve(nodes.size());
    indeg.reserve(nodes.size());

    for (NodeID n : nodes) {
        adj[n];        // ensure present (even if isolated)
        indeg[n] = 0;  // default in-degree
    }

    for (const Edge& e : edges) {
        // Ignore edges that reference unknown nodes to stay robust.
        if (indeg.find(e.from) == indeg.end() || indeg.find(e.to) == indeg.end()) {
            continue;
        }
        adj[e.from].push_back(e.to);
        ++indeg[e.to];
    }

    // Seed the queue with all zero-in-degree nodes.
    std::queue<NodeID> ready;
    for (const auto& kv : indeg) {
        if (kv.second == 0) ready.push(kv.first);
    }

    result.order.reserve(nodes.size());
    while (!ready.empty()) {
        const NodeID n = ready.front();
        ready.pop();
        result.order.push_back(n);

        for (NodeID m : adj[n]) {
            if (--indeg[m] == 0) {
                ready.push(m);
            }
        }
    }

    // If we didn't visit every node, there is a cycle.
    if (result.order.size() != nodes.size()) {
        result.ok = false;
        result.order.clear();
        return result;
    }

    result.ok = true;
    return result;
}

} // namespace sonicpatch
