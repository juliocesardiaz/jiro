//
// AudioGraph.cpp
//
#include "AudioGraph.hpp"
#include "../Nodes/Node.hpp"

namespace sonicpatch {

AudioGraph::AudioGraph() = default;

AudioGraph::~AudioGraph() {
    // Free the live config (no audio thread should be running at destruction).
    GraphConfig* live = current_.exchange(nullptr, std::memory_order_acquire);
    delete live;
    retired_.clear();
}

void AudioGraph::publish(std::unique_ptr<GraphConfig> next) {
    if (next) {
        next->generation = generationCounter_.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    // Release-store the new pointer so the audio thread, on its next acquire-load,
    // sees a fully-constructed config (the writes that built `next` happen-before
    // the release store).
    GraphConfig* raw = next.release();
    GraphConfig* old = current_.exchange(raw, std::memory_order_acq_rel);

    // The old config may still be in use by the audio thread *right now* (it may
    // have acquire-loaded the pointer just before our exchange). We therefore do
    // NOT free it here; we park it for deferred retirement.
    if (old) {
        retired_.emplace_back(old);
    }
}

void AudioGraph::process(const AudioFormat& fmt, uint32_t frames) noexcept {
    // [RT] acquire-load: pairs with the release-store in publish(). After this
    // load, every field the control thread wrote into the config is visible.
    GraphConfig* cfg = current_.load(std::memory_order_acquire);
    if (cfg == nullptr) {
        return; // nothing published yet
    }

    const double sr = fmt.sampleRate;

    // Walk nodes in topological order. Each node's inputs are guaranteed filled
    // because every producer precedes it in `order`.
    for (NodeBinding& binding : cfg->order) {
        if (binding.node == nullptr) {
            continue;
        }

        ProcessContext ctx;
        ctx.inputs     = binding.inputs.empty()  ? nullptr : binding.inputs.data();
        ctx.numInputs  = static_cast<int>(binding.inputs.size());
        ctx.outputs    = binding.outputs.empty() ? nullptr : binding.outputs.data();
        ctx.numOutputs = static_cast<int>(binding.outputs.size());
        ctx.frames     = frames;
        ctx.sampleRate = sr;

        binding.node->process(ctx); // [RT] virtual dispatch, no allocation
    }
}

void AudioGraph::retireOldConfigs() {
    // In a single-publisher / single-consumer RCU scheme, by the time we are
    // called again the audio thread has already acquire-loaded a config newer
    // than everything in `retired_` except possibly the most-recently-retired
    // one. To stay strictly safe we keep the most recent retired config parked
    // for one extra cycle and free the rest.
    //
    // Production hardening (Phase 2): replace this with an epoch/grace-period
    // counter incremented by the audio thread each block so we can prove the
    // audio thread is no longer touching a given pointer before freeing it.
    if (retired_.size() <= 1) {
        return; // keep at least the newest retired config as a grace buffer
    }
    // Free everything except the last (newest) retired config.
    retired_.erase(retired_.begin(), retired_.end() - 1);
}

uint64_t AudioGraph::liveGeneration() const noexcept {
    GraphConfig* cfg = current_.load(std::memory_order_acquire);
    return cfg ? cfg->generation : 0;
}

} // namespace sonicpatch
