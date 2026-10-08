#include "../Audio/AudioControl.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <vector>

static size_t allocations = 0;
void *operator new(size_t n) {
    ++allocations;
    if (void *p = std::malloc(n)) return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void *operator new(size_t n, const std::nothrow_t &) noexcept {
    ++allocations;
    return std::malloc(n);
}
void operator delete(void *p, const std::nothrow_t &) noexcept { std::free(p); }
static int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " #x "\n"; std::exit(1); } } while (0)
static bool near(float a, float b) { return std::abs(a - b) < 0.00005f; }
static SPBuffer buffer(std::vector<float> &v, uint32_t channels) {
    return {v.data(), channels, static_cast<uint32_t>(v.size() * sizeof(float))};
}
static void render(SPStrip *s, std::vector<float> &in, std::vector<float> &out) {
    auto i = buffer(in, 2), o = buffer(out, 2);
    SPStripProcess(s, &i, 1, &o, 1);
}

int main() {
    CHECK(!SPStripCreate(0, 0));
    CHECK(!SPStripCreate(std::numeric_limits<double>::quiet_NaN(), 0));
    auto *s = SPStripCreate(48000, 0);
    CHECK(s);
    std::vector<float> in(1024, 0.8f), out(1024, -999);
    render(s, in, out);
    CHECK(near(out.back(), 0.8f)); // unity center; no 3 dB dip
    CHECK(out[0] > 0 && out[0] < 0.01f); // start fade, not a step
    SPStripTakePeak(s);
    SPStripSetControls(s, 0.25f, 0, false);
    render(s, in, out);
    CHECK(near(out.back(), 0.2f));
    render(s, in, out);
    SPStripTakePeak(s);
    render(s, in, out);
    CHECK(near(SPStripTakePeak(s), 0.2f)); // post-fader peak
    CHECK(SPStripTakePeak(s) == 0);
    SPStripSetControls(s, 1, -1, false);
    render(s, in, out);
    CHECK(near(out[1022], 0.8f) && near(out[1023], 0));
    SPStripSetControls(s, 1, 1, false);
    render(s, in, out);
    CHECK(near(out[1022], 0) && near(out[1023], 0.8f));
    SPStripSetControls(s, 1, 0, true);
    render(s, in, out);
    CHECK(near(out[1022], 0) && near(out[1023], 0));
    SPStripSetControls(s, 1, 0, false);
    render(s, in, out);
    CHECK(near(out[1022], 0.8f) && near(out[1023], 0.8f));

    // Same transition, 32-frame and 512-frame callbacks: identical samples.
    auto *small = SPStripCreate(48000, 0), *large = SPStripCreate(48000, 0);
    std::vector<float> joined;
    std::vector<float> chunkIn(64, 0.8f), chunkOut(64);
    for (int j = 0; j < 16; ++j) { render(small, chunkIn, chunkOut); joined.insert(joined.end(), chunkOut.begin(), chunkOut.end()); }
    render(large, in, out);
    CHECK(joined.size() == out.size());
    for (size_t j = 0; j < out.size(); ++j) CHECK(near(out[j], joined[j]));
    SPStripSetControls(small, 0, 0, true);
    joined.clear();
    for (int j = 0; j < 16; ++j) { render(small, chunkIn, chunkOut); joined.insert(joined.end(), chunkOut.begin(), chunkOut.end()); }
    SPStripSetControls(large, 0, 0, true);
    render(large, in, out);
    for (size_t j = 0; j < out.size(); ++j) CHECK(near(out[j], joined[j]));
    SPStripDestroy(small); SPStripDestroy(large);

    // Planar to interleaved with microphone channels ahead of the tap. The
    // physical-input sentinel must never be forwarded to speakers.
    auto *duplex = SPStripCreate(44100, 1);
    std::vector<float> mic(512, 99), left(512, 0.2f), right(512, -0.4f);
    SPBuffer inputs[] = {buffer(mic, 1), buffer(left, 1), buffer(right, 1)};
    auto ob = buffer(out, 2);
    SPStripProcess(duplex, inputs, 3, &ob, 1);
    CHECK(near(out[1022], 0.2f) && near(out[1023], -0.4f));
    CHECK(std::all_of(out.begin(), out.end(), [](float x) { return std::abs(x) <= 0.4001f; }));
    SPStripDestroy(duplex);

    // Interleaved to planar; null right input and short input must silence.
    std::vector<float> outL(512, 99), outR(512, 99), extra(512, 99);
    SPBuffer outputs[] = {buffer(outL, 1), buffer(outR, 1), buffer(extra, 1)};
    auto ib = buffer(in, 2);
    SPStripProcess(s, &ib, 1, outputs, 3);
    CHECK(near(outL.back(), 0.8f) && near(outR.back(), 0.8f));
    CHECK(std::all_of(extra.begin(), extra.end(), [](float x) { return x == 0; }));
    std::vector<float> shortL(10, 0.5f);
    SPBuffer missing[] = {buffer(shortL, 1), {nullptr, 1, 40}};
    SPStripProcess(s, missing, 2, outputs, 3);
    CHECK(near(outL[9], 0.5f) && outL[10] == 0 && outL.back() == 0);
    CHECK(std::all_of(outR.begin(), outR.end(), [](float x) { return x == 0; }));
    std::fill(out.begin(), out.end(), 99);
    SPStripProcess(s, nullptr, 0, &ob, 1);
    CHECK(std::all_of(out.begin(), out.end(), [](float x) { return x == 0; }));

    // Independent app states: muting one does not alter the other.
    auto *other = SPStripCreate(48000, 0);
    SPStripSetControls(s, 0, 0, true);
    render(s, in, out); CHECK(near(out.back(), 0));
    render(other, in, out); CHECK(near(out.back(), 0.8f));
    SPStripDestroy(other);

    SPStripSetControls(s, std::numeric_limits<float>::quiet_NaN(), 0, false);
    render(s, in, out);
    CHECK(std::all_of(out.begin(), out.end(), [](float x) { return std::isfinite(x); }));
    SPStripSetControls(s, 100, 0, false);
    in[700] = std::numeric_limits<float>::infinity();
    render(s, in, out);
    CHECK(out[700] == 0 && near(out.back(), 0.8f));

    const size_t before = allocations;
    for (int j = 0; j < 1000; ++j) render(s, in, out);
    CHECK(allocations == before);
    CHECK(SPStripCallbackCount(s) >= 1000);
    CHECK(!SPStripHasFormatFault(s));
    SPStripDestroy(s);
    std::cout << "PASS: " << checks << " audio assertions (gain, mute, balance, ramps, layouts, isolation, RT allocation)\n";
}
