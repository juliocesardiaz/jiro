//
// AudioGraph.cpp
//
#include "AudioGraph.hpp"
#include "../Nodes/Node.hpp"

#include <algorithm>

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
    // have acquire-loaded the pointer just before our exchange). Park it stamped
    // with the current render epoch; retireOldConfigs() frees it only once the
    // audio thread has started >= 2 callbacks after this point.
    if (old) {
        RetiredConfig rc;
        rc.config.reset(old);
        rc.epochAtRetire = renderEpoch_.load(std::memory_order_acquire);
        retired_.push_back(std::move(rc));
    }
}

void AudioGraph::process(const AudioFormat& fmt, uint32_t frames) noexcept {
    // [RT] Advance the render epoch FIRST, then load the config. The ordering
    // matters for reclamation: a control thread that observes epoch >= E+2 knows
    // the callback that ran at epoch <= E (and might have held an old config)
    // has completed — callbacks are sequential on the single audio thread, and
    // the intervening callback re-loaded `current_` after the publisher swapped
    // it. fetch_add on a uint64 is lock-free on arm64/x86_64.
    renderEpoch_.fetch_add(1, std::memory_order_acq_rel);

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
    // Epoch-based reclamation. A retired config was superseded before its
    // `epochAtRetire` stamp was taken, so the audio thread could have been at
    // most *inside the callback running at that epoch* while still holding it.
    // Once the epoch has advanced by >= 2, at least one full callback boundary
    // has passed: the potentially-holding callback finished, and every later
    // callback acquire-loaded a pointer published after the swap. Freeing the
    // config (and, via GraphConfig::ownedNodes, any nodes only it kept alive)
    // is then provably safe.
    //
    // If the audio thread is not running the epoch never advances and configs
    // are held here indefinitely; the engine calls drain() in that case.
    const uint64_t epochNow = renderEpoch_.load(std::memory_order_acquire);
    auto stillInGrace = [epochNow](const RetiredConfig& rc) {
        return epochNow < rc.epochAtRetire + 2;
    };
    retired_.erase(
        std::remove_if(retired_.begin(), retired_.end(),
                       [&](const RetiredConfig& rc) { return !stillInGrace(rc); }),
        retired_.end());
}

void AudioGraph::drain() {
    // Caller guarantees no audio thread is inside process().
    retired_.clear();
}

uint64_t AudioGraph::liveGeneration() const noexcept {
    GraphConfig* cfg = current_.load(std::memory_order_acquire);
    return cfg ? cfg->generation : 0;
}

} // namespace sonicpatch
