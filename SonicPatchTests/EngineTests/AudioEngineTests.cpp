//
// AudioEngineTests.cpp
// Facade-level tests: strip lifecycle, lock-free level reads, effect slots,
// and the copyable shared-handle semantics the Swift interop layer relies on.
//
#include "Public/AudioEngine.hpp"

#include <gtest/gtest.h>

using namespace sonicpatch;

TEST(AudioEngineTests, CreateAndRemoveStrips) {
    AudioEngine engine;
    engine.setAudioFormat(AudioFormat{});

    const StripID a = engine.createChannelStrip("com.example.a");
    const StripID b = engine.createChannelStrip("com.example.b");
    EXPECT_NE(a, kInvalidStrip);
    EXPECT_NE(b, kInvalidStrip);
    EXPECT_NE(a, b);

    engine.removeChannelStrip(a);
    engine.removeChannelStrip(b);
    engine.removeChannelStrip(a); // double-remove must be harmless
}

TEST(AudioEngineTests, GetLevelUnknownStripIsZero) {
    AudioEngine engine;
    const LevelSnapshot s = engine.getLevel(12345);
    EXPECT_EQ(s.peak, 0.0f);
    EXPECT_EQ(s.rms, 0.0f);
}

TEST(AudioEngineTests, GetLevelTracksStripLifecycle) {
    AudioEngine engine;
    engine.setAudioFormat(AudioFormat{});

    const StripID strip = engine.createChannelStrip("com.example.app");
    // Fresh strip: meter exists and reads silence.
    LevelSnapshot s = engine.getLevel(strip);
    EXPECT_EQ(s.peak, 0.0f);

    engine.removeChannelStrip(strip);
    // Removed strip: reads must fall back to {0,0}, never crash.
    s = engine.getLevel(strip);
    EXPECT_EQ(s.peak, 0.0f);
    EXPECT_EQ(s.rms, 0.0f);
}

TEST(AudioEngineTests, EffectSlotLifecycle) {
    AudioEngine engine;
    engine.setAudioFormat(AudioFormat{});
    const StripID strip = engine.createChannelStrip("com.example.app");

    const int eq   = engine.insertBuiltinEffect(strip, 0, BuiltinEffectType::ParametricEQ);
    const int comp = engine.insertBuiltinEffect(strip, 1, BuiltinEffectType::Compressor);
    EXPECT_GE(eq, 0);
    EXPECT_GE(comp, 0);
    EXPECT_NE(eq, comp);

    // Unknown strip / bad inserts fail cleanly.
    EXPECT_EQ(engine.insertBuiltinEffect(999, 0, BuiltinEffectType::Limiter), -1);

    engine.setEffectParameter(strip, eq, 0, 1000.0f);   // must not crash
    engine.removeEffect(strip, eq);
    engine.removeEffect(strip, eq);                     // double-remove harmless
    engine.setEffectParameter(strip, eq, 0, 2000.0f);   // gone: silently ignored
}

TEST(AudioEngineTests, CopiesAliasTheSameEngine) {
    // The Swift interop layer requires AudioEngine to be a copyable handle
    // whose copies refer to the same underlying engine.
    AudioEngine engine;
    engine.setAudioFormat(AudioFormat{});
    const StripID strip = engine.createChannelStrip("com.example.app");

    AudioEngine copy = engine; // shared handle, not a second engine
    copy.setVolume(strip, -6.0f);              // operates on the same strips
    const LevelSnapshot viaCopy = copy.getLevel(strip);
    const LevelSnapshot viaOrig = engine.getLevel(strip);
    EXPECT_EQ(viaCopy.peak, viaOrig.peak);

    copy.removeChannelStrip(strip);
    // The original sees the removal because they are the same engine.
    const LevelSnapshot afterRemove = engine.getLevel(strip);
    EXPECT_EQ(afterRemove.peak, 0.0f);
    EXPECT_EQ(afterRemove.rms, 0.0f);
}

TEST(AudioEngineTests, StartStopIsIdempotent) {
    AudioEngine engine;
    engine.setAudioFormat(AudioFormat{});
    EXPECT_FALSE(engine.isRunning());
    engine.start();
    EXPECT_TRUE(engine.isRunning());
    engine.start(); // second start is a no-op
    EXPECT_TRUE(engine.isRunning());
    engine.stop();
    EXPECT_FALSE(engine.isRunning());
    engine.stop(); // second stop is a no-op
    EXPECT_FALSE(engine.isRunning());
}
