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

// ---------------------------------------------------------------------------
// Epoch-based reclamation (regression tests for the retire-while-in-use UAF).
// ---------------------------------------------------------------------------

namespace {

// Builds a config that shares ownership of `n` (as published GraphConfigs do).
std::unique_ptr<GraphConfig> makeOwningConfig(std::shared_ptr<Node> n) {
    auto cfg = std::make_unique<GraphConfig>();
    cfg->format = AudioFormat{};
    NodeBinding b;
    b.node = n.get();
    cfg->order.push_back(std::move(b));
    cfg->ownedNodes.push_back(std::move(n));
    return cfg;
}

} // namespace

TEST(AudioGraphTests, RetireHoldsConfigUntilEpochAdvances) {
    // The original bug: retireOldConfigs() freed a config the audio thread
    // could still be inside. The epoch protocol must hold a retired config
    // until the render epoch has advanced by >= 2 past its retirement stamp.
    AudioGraph g;
    auto nodeA = std::make_shared<CountingNode>(1);
    std::weak_ptr<Node> aAlive = nodeA;

    g.publish(makeOwningConfig(std::move(nodeA)));       // A live
    g.publish(makeOwningConfig(std::make_shared<CountingNode>(2))); // A retired @ epoch 0

    // Rapid consecutive control-thread retire attempts (the crash scenario:
    // two publishes + retires within one render callback) must NOT free A —
    // the audio thread has not advanced the epoch since A was retired.
    g.retireOldConfigs();
    g.retireOldConfigs();
    EXPECT_FALSE(aAlive.expired()) << "config freed while potentially in use";
    EXPECT_EQ(g.retiredCount(), 1u);

    // One callback (epoch 1) is not enough either: the callback running when A
    // was retired could have been epoch 0's, and epoch 1 only proves epoch 0
    // *started* before it. Two advances prove the holding callback completed.
    AudioFormat fmt;
    g.process(fmt, 128); // epoch 1
    g.retireOldConfigs();
    EXPECT_FALSE(aAlive.expired());

    g.process(fmt, 128); // epoch 2
    g.retireOldConfigs();
    EXPECT_TRUE(aAlive.expired()) << "config not reclaimed after grace period";
    EXPECT_EQ(g.retiredCount(), 0u);
}

TEST(AudioGraphTests, DrainReclaimsEverythingWhenNoAudioThread) {
    // With no audio thread the epoch never advances; drain() (callable only
    // when provably idle) must still reclaim all retired configs.
    AudioGraph g;
    auto nodeA = std::make_shared<CountingNode>(1);
    std::weak_ptr<Node> aAlive = nodeA;

    g.publish(makeOwningConfig(std::move(nodeA)));
    g.publish(makeOwningConfig(std::make_shared<CountingNode>(2)));

    g.retireOldConfigs();            // epoch never advanced -> must hold
    EXPECT_FALSE(aAlive.expired());

    g.drain();
    EXPECT_TRUE(aAlive.expired());
    EXPECT_EQ(g.retiredCount(), 0u);
}

TEST(AudioGraphTests, ConfigOwnershipKeepsNodeAliveAfterExternalRelease) {
    // Regression for the node-lifetime hole: the control side (a Strip) may
    // drop its reference to a node at any time; a node bound in the live
    // config must stay alive until that config is reclaimed.
    AudioGraph g;
    auto node = std::make_shared<CountingNode>(7);
    std::weak_ptr<Node> alive = node;

    g.publish(makeOwningConfig(node));
    node.reset(); // control side lets go (strip removed)
    EXPECT_FALSE(alive.expired()) << "live config must co-own its nodes";

    AudioFormat fmt;
    g.process(fmt, 64); // the node is still safely reachable
    ASSERT_FALSE(alive.expired());

    // Replace and reclaim: only after the epoch grace does the node die.
    g.publish(makeOwningConfig(std::make_shared<CountingNode>(8)));
    g.process(fmt, 64);
    g.process(fmt, 64);
    g.retireOldConfigs();
    EXPECT_TRUE(alive.expired());
}
