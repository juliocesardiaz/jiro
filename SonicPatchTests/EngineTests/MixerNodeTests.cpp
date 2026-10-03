//
// MixerNodeTests.cpp
// Regression tests for the mixer's de-zipper ramp — most importantly that the
// ramp state persists across block boundaries (the original code snapped the
// gain to its target at the end of every block, producing an audible click
// with callbacks shorter than the 64-sample ramp).
//
#include "Nodes/MixerNode.hpp"
#include "TestSupport.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

using namespace sonicpatch;

namespace {

// Renders `blocks` consecutive blocks of `blockFrames` each through a 1-input
// mixer fed a constant 1.0 signal, and returns the concatenated mono output —
// i.e. the gain trajectory actually applied.
std::vector<float> renderGainTrajectory(MixerNode& mixer,
                                        uint32_t blockFrames,
                                        uint32_t blocks) {
    std::vector<float> inBuf(blockFrames, 1.0f);
    std::vector<float> outBuf(blockFrames, 0.0f);
    std::vector<float> trajectory;
    trajectory.reserve(blockFrames * blocks);

    for (uint32_t b = 0; b < blocks; ++b) {
        float* inChans[1]  = {inBuf.data()};
        float* outChans[1] = {outBuf.data()};
        AudioBuffer in(inChans, 1, blockFrames);
        AudioBuffer out(outChans, 1, blockFrames);

        ProcessContext ctx = test::makeContext(&in, &out, blockFrames);
        mixer.process(ctx);

        trajectory.insert(trajectory.end(), outBuf.begin(), outBuf.end());
    }
    return trajectory;
}

float maxAdjacentStep(const std::vector<float>& v) {
    float maxStep = 0.0f;
    for (size_t i = 1; i < v.size(); ++i) {
        maxStep = std::max(maxStep, std::fabs(v[i] - v[i - 1]));
    }
    return maxStep;
}

} // namespace

TEST(MixerNodeTests, UnityGainPassThrough) {
    MixerNode mixer(1, 4);
    mixer.prepare(AudioFormat{});
    auto out = renderGainTrajectory(mixer, 128, 2);
    for (float s : out) EXPECT_FLOAT_EQ(s, 1.0f);
}

TEST(MixerNodeTests, RampHasNoDiscontinuityAtSmallBlockSizes) {
    // The original bug: with 32-frame blocks and a 64-sample ramp, the block
    // ended halfway through the ramp and then snapped to the target — a 0.5
    // full-scale step between adjacent samples. The per-sample ramp step is
    // ~1/64, so nothing near 0.5 may ever appear.
    MixerNode mixer(1, 4);
    mixer.prepare(AudioFormat{});

    // Settle at gain 1, then command a full cut and render in 32-frame blocks.
    (void)renderGainTrajectory(mixer, 32, 1);
    mixer.setInputGain(0, 0.0f);
    auto out = renderGainTrajectory(mixer, 32, 8);

    const float step = maxAdjacentStep(out);
    EXPECT_LT(step, 0.05f) << "gain snapped at a block boundary (click)";

    // And the ramp must actually converge onto the target.
    EXPECT_NEAR(out.back(), 0.0f, 1e-3f);
}

TEST(MixerNodeTests, RampCompletesWithinLargeBlock) {
    // With a block longer than the ramp, the gain must reach the target inside
    // the block and hold it.
    MixerNode mixer(1, 4);
    mixer.prepare(AudioFormat{});

    (void)renderGainTrajectory(mixer, 256, 1); // settle at 1.0
    mixer.setInputGain(0, 0.5f);
    auto out = renderGainTrajectory(mixer, 256, 1);

    EXPECT_NEAR(out[MixerNode::kRampSamples - 1], 0.5f, 1e-3f);
    EXPECT_FLOAT_EQ(out.back(), out[MixerNode::kRampSamples - 1]);
    EXPECT_LT(maxAdjacentStep(out), 0.05f);
}

TEST(MixerNodeTests, SumsMultipleInputs) {
    MixerNode mixer(1, 4);
    mixer.prepare(AudioFormat{});

    const uint32_t frames = 64;
    std::vector<float> a(frames, 0.25f);
    std::vector<float> b(frames, 0.5f);
    std::vector<float> outBuf(frames, 0.0f);

    float* aChan[1]   = {a.data()};
    float* bChan[1]   = {b.data()};
    float* outChan[1] = {outBuf.data()};
    AudioBuffer inputs[2] = {AudioBuffer(aChan, 1, frames),
                             AudioBuffer(bChan, 1, frames)};
    AudioBuffer out(outChan, 1, frames);

    ProcessContext ctx = test::makeContext(inputs, &out, frames);
    ctx.numInputs = 2; // two-input sum: builder covers the 1-in/1-out shape
    mixer.process(ctx);

    for (float s : outBuf) EXPECT_FLOAT_EQ(s, 0.75f);
}
