//
// BiquadTests.cpp
// Sanity checks on the RBJ biquad coefficient design + processing.
//
#include "DSP/Biquad.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

using namespace sonicpatch;

namespace {
constexpr double kSampleRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;

// Measure steady-state output amplitude of a biquad for a sine at `freq`.
// Drives the filter with enough cycles to settle, then measures peak over the
// last portion.
float measureGain(const BiquadCoeffs& c, double freq, int cycles = 200) {
    Biquad bq;
    bq.setCoeffs(c);
    bq.reset();

    const double w = 2.0 * kPi * freq / kSampleRate;
    const int samplesPerCycle = static_cast<int>(kSampleRate / freq);
    const int total = std::max(samplesPerCycle * cycles, 4096);

    float peak = 0.0f;
    const int settle = total / 2;
    for (int n = 0; n < total; ++n) {
        const float x = static_cast<float>(std::sin(w * n));
        const float y = bq.processSample(x);
        if (n >= settle) peak = std::max(peak, std::fabs(y));
    }
    return peak; // input peak is ~1.0, so this is the linear gain
}
} // namespace

TEST(BiquadTests, LowPassPassesLowFrequencyAttenuatesHigh) {
    auto c = BiquadCoeffs::design(BiquadType::LowPass, kSampleRate, 1000.0, 0.707);

    const float lowGain  = measureGain(c, 100.0);    // well below cutoff
    const float highGain = measureGain(c, 12000.0);  // well above cutoff

    EXPECT_NEAR(lowGain, 1.0f, 0.05f);   // passband ~ unity
    EXPECT_LT(highGain, 0.2f);           // strongly attenuated
}

TEST(BiquadTests, LowPassPassesDC) {
    // At DC (z=1), H(1) = (b0+b1+b2)/(1+a1+a2). For a lowpass this should be ~1.
    auto c = BiquadCoeffs::design(BiquadType::LowPass, kSampleRate, 1000.0, 0.707);
    const float dcGain = (c.b0 + c.b1 + c.b2) / (1.0f + c.a1 + c.a2);
    EXPECT_NEAR(dcGain, 1.0f, 1e-3f);
}

TEST(BiquadTests, HighPassRejectsDC) {
    auto c = BiquadCoeffs::design(BiquadType::HighPass, kSampleRate, 1000.0, 0.707);
    const float dcGain = (c.b0 + c.b1 + c.b2) / (1.0f + c.a1 + c.a2);
    EXPECT_NEAR(dcGain, 0.0f, 1e-3f);

    const float highGain = measureGain(c, 12000.0);
    EXPECT_NEAR(highGain, 1.0f, 0.1f); // passband ~ unity
}

TEST(BiquadTests, PeakingBoostsAtCenterFrequency) {
    const double fc = 1000.0;
    auto c = BiquadCoeffs::design(BiquadType::Peaking, kSampleRate, fc, 1.0, /*gainDb=*/12.0);

    const float centerGain = measureGain(c, fc);
    // +12 dB ~ 3.98x linear. Allow tolerance for the measurement method.
    EXPECT_NEAR(centerGain, 3.98f, 0.4f);

    // Far from the center frequency, gain should be near unity.
    const float farGain = measureGain(c, 60.0);
    EXPECT_NEAR(farGain, 1.0f, 0.2f);
}

TEST(BiquadTests, PeakingCutAtCenterFrequency) {
    const double fc = 2000.0;
    auto c = BiquadCoeffs::design(BiquadType::Peaking, kSampleRate, fc, 1.0, /*gainDb=*/-12.0);
    const float centerGain = measureGain(c, fc);
    EXPECT_LT(centerGain, 0.5f); // attenuated relative to unity
}
