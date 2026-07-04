#pragma once
//
// TestSupport.hpp
// Shared builders for node-level tests: wrap raw channel storage in
// AudioBuffer views and assemble a ProcessContext without repeating the
// pointer choreography in every test file. If ProcessContext grows a field
// (e.g. Phase-2 buffer indices), update it here once.
//
#include "Core/AudioBuffer.hpp"
#include "Nodes/Node.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace sonicpatch {
namespace test {

/// Owns channel storage + pointer table for one mono/stereo buffer, and
/// exposes an AudioBuffer view over it. Keep the instance alive while the
/// view is in use.
struct OwnedBuffer {
    explicit OwnedBuffer(uint32_t channels, uint32_t frames, float fill = 0.0f)
        : storage(channels, std::vector<float>(frames, fill)) {
        for (auto& ch : storage) ptrs.push_back(ch.data());
        view = AudioBuffer(ptrs.data(), channels, frames);
    }

    std::vector<std::vector<float>> storage;
    std::vector<float*>             ptrs;
    AudioBuffer                     view;
};

/// A ProcessContext over one input and (optionally) one output buffer.
inline ProcessContext makeContext(AudioBuffer* input,
                                  AudioBuffer* output,
                                  uint32_t frames,
                                  double sampleRate = 48000.0) {
    ProcessContext ctx;
    ctx.inputs     = input;
    ctx.numInputs  = input ? 1 : 0;
    ctx.outputs    = output;
    ctx.numOutputs = output ? 1 : 0;
    ctx.frames     = frames;
    ctx.sampleRate = sampleRate;
    return ctx;
}

} // namespace test
} // namespace sonicpatch
