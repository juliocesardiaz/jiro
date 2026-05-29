//
// AudioGraphTests.cpp
//
#include "Core/AudioGraph.hpp"
#include "Core/GraphConfig.hpp"
#include "Nodes/Node.hpp"

#include <gtest/gtest.h>

#include <memory>

using namespace sonicpatch;

namespace {

// Minimal test node that counts how many times process() ran.
class CountingNode final : public Node {
public:
    explicit CountingNode(NodeID id) : Node(NodeKind::Mixer, id) {}
    void prepare(const AudioFormat&) override {}
    void process(ProcessContext&) override { ++count; }
    int count = 0;
};

std::unique_ptr<GraphConfig> makeConfig(Node* n) {
    auto cfg = std::make_unique<GraphConfig>();
    cfg->format = AudioFormat{};
    NodeBinding b;
    b.node = n;
    cfg->order.push_back(std::move(b));
    return cfg;
}

} // namespace

TEST(AudioGraphTests, ProcessWithNoConfigIsNoOp) {
    AudioGraph g;
    AudioFormat fmt;
    EXPECT_NO_THROW(g.process(fmt, 512)); // nothing published yet
    EXPECT_EQ(g.liveGeneration(), 0u);
}

TEST(AudioGraphTests, PublishMakesConfigLive) {
    AudioGraph g;
    CountingNode node(1);

    g.publish(makeConfig(&node));
    EXPECT_EQ(g.liveGeneration(), 1u);

    AudioFormat fmt;
    g.process(fmt, 256);
    g.process(fmt, 256);
    EXPECT_EQ(node.count, 2);
}

TEST(AudioGraphTests, RepublishReturnsLatestConfig) {
    AudioGraph g;
    CountingNode first(1);
    CountingNode second(2);

    g.publish(makeConfig(&first));
    AudioFormat fmt;
    g.process(fmt, 128);
    EXPECT_EQ(first.count, 1);

    // Swap in a new config; the audio thread should now run `second`.
    g.publish(makeConfig(&second));
    EXPECT_EQ(g.liveGeneration(), 2u);

    g.process(fmt, 128);
    EXPECT_EQ(second.count, 1);
    EXPECT_EQ(first.count, 1); // first no longer runs

    // Retiring old configs must not affect the live one.
    g.retireOldConfigs();
    g.process(fmt, 128);
    EXPECT_EQ(second.count, 2);
}
