//
// TopologicalSortTests.cpp
//
#include "Core/TopologicalSort.hpp"

#include <gtest/gtest.h>

#include <algorithm>

using namespace sonicpatch;

namespace {
// Returns the position of `id` in the order, or -1 if absent.
int posOf(const std::vector<NodeID>& order, NodeID id) {
    auto it = std::find(order.begin(), order.end(), id);
    return it == order.end() ? -1 : static_cast<int>(it - order.begin());
}
} // namespace

TEST(TopologicalSortTests, LinearChainSortsInOrder) {
    // 1 -> 2 -> 3 -> 4
    std::vector<NodeID> nodes = {1, 2, 3, 4};
    std::vector<Edge>   edges = {{1, 2}, {2, 3}, {3, 4}};

    TopoSortResult r = topologicalSort(nodes, edges);
    ASSERT_TRUE(r.ok);
    ASSERT_EQ(r.order.size(), 4u);

    EXPECT_LT(posOf(r.order, 1), posOf(r.order, 2));
    EXPECT_LT(posOf(r.order, 2), posOf(r.order, 3));
    EXPECT_LT(posOf(r.order, 3), posOf(r.order, 4));
}

TEST(TopologicalSortTests, DiamondRespectsDependencies) {
    //   1 -> 2
    //   1 -> 3
    //   2 -> 4
    //   3 -> 4
    std::vector<NodeID> nodes = {1, 2, 3, 4};
    std::vector<Edge>   edges = {{1, 2}, {1, 3}, {2, 4}, {3, 4}};

    TopoSortResult r = topologicalSort(nodes, edges);
    ASSERT_TRUE(r.ok);
    EXPECT_LT(posOf(r.order, 1), posOf(r.order, 2));
    EXPECT_LT(posOf(r.order, 1), posOf(r.order, 3));
    EXPECT_LT(posOf(r.order, 2), posOf(r.order, 4));
    EXPECT_LT(posOf(r.order, 3), posOf(r.order, 4));
}

TEST(TopologicalSortTests, CycleIsDetected) {
    // 1 -> 2 -> 3 -> 1  (cycle)
    std::vector<NodeID> nodes = {1, 2, 3};
    std::vector<Edge>   edges = {{1, 2}, {2, 3}, {3, 1}};

    TopoSortResult r = topologicalSort(nodes, edges);
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(r.order.empty());
}

TEST(TopologicalSortTests, IsolatedNodesAreIncluded) {
    std::vector<NodeID> nodes = {10, 20, 30};
    std::vector<Edge>   edges = {}; // no edges

    TopoSortResult r = topologicalSort(nodes, edges);
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.order.size(), 3u);
}
