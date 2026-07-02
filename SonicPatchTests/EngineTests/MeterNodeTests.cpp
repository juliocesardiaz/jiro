//
// MeterNodeTests.cpp
// Regression tests for meter ballistics — most importantly that peak decay is
// a real ~300 ms time constant independent of block size (the original code
// applied a per-sample coefficient once per block, freezing the meter).
//
#include "Nodes/MeterNode.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

using namespace sonicpatch;

namespace {

constexpr double   kSampleRate = 48000.0;
constexpr uint32_t kFrames     = 512;

AudioFormat format() {
    AudioFormat fmt;
    fmt.sampleRate      = kSampleRate;
    fmt.channels        = 2;
    fmt.framesPerBuffer = kFrames;
    return fmt;
}

// Feed one stereo block through the meter.
void feedBlock(MeterNode& meter, float* left, float* right, uint32_t frames) {
    float* chans[2] = {left, right};
    AudioBuffer in(chans, 2, frames);
    ProcessContext ctx;
    ctx.inputs     = &in;
    ctx.numInputs  = 1;
    ctx.frames     = frames;
    ctx.sampleRate = kSampleRate;
    meter.process(ctx);
}

} // namespace

TEST(MeterNodeTests, PeakDecaysWithRealTimeConstant) {
    MeterNode meter(1);
    meter.prepare(format());

    // One full-scale block, then one second of silence.
    std::vector<float> loud(kFrames, 1.0f);
    std::vector<float> quiet(kFrames, 0.0f);

    feedBlock(meter, loud.data(), loud.data(), kFrames);
    EXPECT_NEAR(meter.snapshot().peak, 1.0f, 1e-4f);

    const uint32_t blocksPerSecond =
        static_cast<uint32_t>(kSampleRate / kFrames); // ~93
    for (uint32_t i = 0; i < blocksPerSecond; ++i) {
        feedBlock(meter, quiet.data(), quiet.data(), kFrames);
    }

    // With a 300 ms time constant, one second of silence leaves
    // exp(-1/0.3) ~= 0.036. The broken per-block decay left ~0.99.
    const float peak = meter.snapshot().peak;
    EXPECT_LT(peak, 0.06f) << "peak decay far too slow (frozen meter)";
    EXPECT_GT(peak, 0.01f) << "peak decay far too fast";
}

TEST(MeterNodeTests, PeakDecayIsBlockSizeIndependent) {
    // The same one second of silence must decay to (approximately) the same
    // value whether rendered as 512-frame or 64-frame blocks.
    auto decayAfterOneSecond = [](uint32_t blockFrames) {
        MeterNode meter(1);
        AudioFormat fmt = format();
        fmt.framesPerBuffer = blockFrames;
        meter.prepare(fmt);

        std::vector<float> loud(blockFrames, 1.0f);
        std::vector<float> quiet(blockFrames, 0.0f);
        feedBlock(meter, loud.data(), loud.data(), blockFrames);

        const uint32_t blocks = static_cast<uint32_t>(kSampleRate) / blockFrames;
        for (uint32_t i = 0; i < blocks; ++i) {
            feedBlock(meter, quiet.data(), quiet.data(), blockFrames);
        }
        return meter.snapshot().peak;
    };

    const float big   = decayAfterOneSecond(512);
    const float small = decayAfterOneSecond(64);
    EXPECT_NEAR(big, small, 0.01f);
}

TEST(MeterNodeTests, RmsConvergesToSineLevel) {
    MeterNode meter(1);
    meter.prepare(format());

    // 1 kHz full-scale sine for 2 seconds; RMS should approach 1/sqrt(2).
    std::vector<float> block(kFrames);
    double phase = 0.0;
    const double inc = 2.0 * M_PI * 1000.0 / kSampleRate;
    const uint32_t blocks = 2u * static_cast<uint32_t>(kSampleRate / kFrames);
    for (uint32_t b = 0; b < blocks; ++b) {
        for (uint32_t n = 0; n < kFrames; ++n) {
            block[n] = static_cast<float>(std::sin(phase));
            phase += inc;
        }
        feedBlock(meter, block.data(), block.data(), kFrames);
    }

    EXPECT_NEAR(meter.snapshot().rms, 1.0f / std::sqrt(2.0f), 0.02f);
}

TEST(MeterNodeTests, InvalidInputDecaysInsteadOfFreezing) {
    MeterNode meter(1);
    meter.prepare(format());

    std::vector<float> loud(kFrames, 1.0f);
    feedBlock(meter, loud.data(), loud.data(), kFrames);
    ASSERT_NEAR(meter.snapshot().peak, 1.0f, 1e-4f);

    // Input goes away entirely (e.g. tap stops delivering): the meter must
    // fall, not hold its last value.
    ProcessContext ctx;
    ctx.frames     = kFrames;
    ctx.sampleRate = kSampleRate;
    const uint32_t blocksPerSecond = static_cast<uint32_t>(kSampleRate / kFrames);
    for (uint32_t i = 0; i < blocksPerSecond; ++i) {
        meter.process(ctx);
    }
    EXPECT_LT(meter.snapshot().peak, 0.06f);
}
