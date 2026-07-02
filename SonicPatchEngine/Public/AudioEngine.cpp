//
// AudioEngine.cpp
// Implementation of the public facade. All internal C++ types live here behind
// the opaque Impl, so this is the only TU that knows about the graph, nodes, and
// DSP. Mutating methods run on the control/UI thread and (re)build + publish an
// immutable GraphConfig to the audio thread.
//
#include "AudioEngine.hpp"

#include "../Core/AudioGraph.hpp"
#include "../Core/BufferPool.hpp"
#include "../Core/GraphConfig.hpp"
#include "../Core/TopologicalSort.hpp"
#include "../DSP/EffectFactory.hpp"
#include "../Nodes/DeviceSinkNode.hpp"
#include "../Nodes/EffectNode.hpp"
#include "../Nodes/MeterNode.hpp"
#include "../Nodes/MixerNode.hpp"
#include "../Nodes/TapSourceNode.hpp"
#include "../Nodes/VolumeNode.hpp"

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace sonicpatch {

// ============================================================================
// Internal model
// ============================================================================
//
// A "channel strip" is a small chain of internal nodes:
//
//   TapSource -> [Effect slots...] -> Volume -> Meter -> (mix into master)
//
// All strips feed a single master MixerNode, whose output is written by the
// DeviceSink. The strip's effect rack is an ordered list of EffectNodes.
//
// NOTE: This implementation focuses on a correct, allocation-at-setup model and
// the public API contract. The full per-block buffer assignment / pooled-edge
// wiring is built in rebuildGraph(); buffer-pool integration for distinct edges
// is marked TODO where it would otherwise require the live Core Audio I/O sizes.

namespace {

// Node ownership: strips hold shared_ptrs, and every published GraphConfig
// also holds shared_ptrs to the nodes it binds (GraphConfig::ownedNodes).
// Removing a strip therefore never destroys a node the audio thread might
// still reach — the node dies only when the last config referencing it is
// reclaimed by the graph's epoch protocol.
struct EffectSlot {
    int                         handle = -1;
    std::shared_ptr<EffectNode> node;
};

struct Strip {
    StripID                          id = kInvalidStrip;
    std::shared_ptr<TapSourceNode>   source;
    std::vector<EffectSlot>          effects;
    std::shared_ptr<VolumeNode>      volume;
    std::shared_ptr<MeterNode>       meter;
    float                            inputTrimDb = 0.0f;
    int                              nextEffectHandle = 0;
};

} // namespace

// ----------------------------------------------------------------------------

struct AudioEngine::Impl {
    // Control-thread state. The audio thread only touches graph_ (lock-free).
    std::mutex                          mutex_;          ///< serializes control ops
    AudioFormat                         format_;
    std::atomic<bool>                   running_{false};

    AudioGraph                          graph_;
    BufferPool                          pool_;

    std::map<StripID, std::unique_ptr<Strip>> strips_;
    StripID                             nextStrip_ = 1;  ///< 0 is reserved/invalid

    std::shared_ptr<MixerNode>          master_;         ///< sums all strips
    std::shared_ptr<DeviceSinkNode>     sink_;           ///< final output
    NodeID                              nextNodeId_ = 1;

    Impl() {
        // Master mixer is sized to a generous max strip count; we rebuild bindings
        // each publish. 64 input lanes is plenty for a channel-strip mixer UI.
        master_ = std::make_shared<MixerNode>(nextNodeId_++, 64);
        sink_   = std::make_shared<DeviceSinkNode>(nextNodeId_++);
    }

    // Rebuild and publish a fresh immutable GraphConfig from the current strips.
    // Control thread only. Holds mutex_ already.
    void rebuildGraph() {
        prepareAll();

        // Collect nodes + edges for the topological sort (diagnostic / ordering).
        std::vector<NodeID> nodes;
        std::vector<Edge>   edges;
        auto add = [&](Node* n) { if (n) nodes.push_back(n->id()); };

        for (auto& kv : strips_) {
            Strip* s = kv.second.get();
            add(s->source.get());
            Node* prev = s->source.get();
            for (auto& slot : s->effects) {
                add(slot.node.get());
                if (prev && slot.node) edges.push_back({prev->id(), slot.node->id()});
                prev = slot.node.get();
            }
            add(s->volume.get());
            if (prev && s->volume) edges.push_back({prev->id(), s->volume->id()});
            add(s->meter.get());
            if (s->volume && s->meter) edges.push_back({s->volume->id(), s->meter->id()});
            if (s->meter) edges.push_back({s->meter->id(), master_->id()});
        }
        add(master_.get());
        add(sink_.get());
        edges.push_back({master_->id(), sink_->id()});

        const TopoSortResult topo = topologicalSort(nodes, edges);
        // A strip chain + master + sink is always a DAG; a failure would indicate
        // a logic bug. We still guard against it.
        if (!topo.ok) {
            return; // keep the previously-published config
        }

        // Build the binding list in sorted order. Buffer assignment to pooled
        // edges is performed here in a full implementation; see TODO below.
        auto cfg = std::make_unique<GraphConfig>();
        cfg->format = format_;

        // Map node id -> shared_ptr for binding lookup. The config takes shared
        // ownership of every node it binds so that strip/effect removal on the
        // control thread can never free a node out from under the audio thread.
        std::map<NodeID, std::shared_ptr<Node>> byId;
        for (auto& kv : strips_) {
            Strip* s = kv.second.get();
            if (s->source) byId[s->source->id()] = s->source;
            for (auto& slot : s->effects) if (slot.node) byId[slot.node->id()] = slot.node;
            if (s->volume) byId[s->volume->id()] = s->volume;
            if (s->meter)  byId[s->meter->id()]  = s->meter;
        }
        byId[master_->id()] = master_;
        byId[sink_->id()]   = sink_;

        for (NodeID nid : topo.order) {
            auto it = byId.find(nid);
            if (it == byId.end()) continue;
            NodeBinding b;
            b.node = it->second.get();
            cfg->ownedNodes.push_back(it->second);
            // TODO(Phase 2): acquire pooled buffers from pool_ and populate
            // b.inputs / b.outputs so each edge has dedicated storage. This
            // requires knowing the live channel count from the Tap/device, which
            // is established once Core Audio I/O is wired (Phase 1). For now the
            // bindings carry no buffers; AudioGraph::process tolerates this.
            cfg->order.push_back(std::move(b));
        }

        graph_.publish(std::move(cfg));

        // Reclaim retired configs: with a live audio thread only epoch-expired
        // configs are freed; with no audio thread running nothing can be inside
        // process(), so draining everything is safe.
        if (running_.load()) {
            graph_.retireOldConfigs();
        } else {
            graph_.drain();
        }
    }

    void prepareAll() {
        // prepareIfNeeded, not prepare: nodes already prepared for the current
        // format are skipped. prepare() reallocates internal state, which must
        // never happen to a node the audio thread may be executing (nodes bound
        // in the currently-published config). Format changes are only allowed
        // while stopped, so live nodes are never re-prepared here.
        for (auto& kv : strips_) {
            Strip* s = kv.second.get();
            if (s->source) s->source->prepareIfNeeded(format_);
            for (auto& slot : s->effects) if (slot.node) slot.node->prepareIfNeeded(format_);
            if (s->volume) s->volume->prepareIfNeeded(format_);
            if (s->meter)  s->meter->prepareIfNeeded(format_);
        }
        master_->prepareIfNeeded(format_);
        sink_->prepareIfNeeded(format_);
    }

    Strip* find(StripID id) {
        auto it = strips_.find(id);
        return it == strips_.end() ? nullptr : it->second.get();
    }
};

// ============================================================================
// Facade
// ============================================================================

AudioEngine::AudioEngine() : impl_(std::make_shared<Impl>()) {}
AudioEngine::~AudioEngine() = default;

AudioEngine::AudioEngine(AudioEngine&&) noexcept            = default;
AudioEngine& AudioEngine::operator=(AudioEngine&&) noexcept = default;

void AudioEngine::start() {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    if (impl_->running_.load()) return;
    impl_->rebuildGraph();
    // TODO(Phase 1): create + start the Tap IOProcs and the output device
    // IOProc here. Those callbacks drive AudioGraph::process on the RT thread.
    impl_->running_.store(true);
}

void AudioEngine::stop() {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    if (!impl_->running_.load()) return;
    // TODO(Phase 1): stop + dispose the IOProcs before tearing anything down.
    impl_->running_.store(false);
    // With the IOProcs stopped no audio thread can be inside process(); reclaim
    // every retired config (and any nodes only they kept alive) immediately.
    impl_->graph_.drain();
}

bool AudioEngine::isRunning() const {
    return impl_->running_.load();
}

void AudioEngine::setAudioFormat(AudioFormat fmt) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->format_ = fmt;
    // Reserve the buffer pool: enough buffers for a generous number of edges,
    // 2x over-provisioned internally. countPerSize is the per-size-class count.
    impl_->pool_.reserve(/*countPerSize=*/64);
    if (impl_->running_.load()) {
        impl_->rebuildGraph();
    }
}

StripID AudioEngine::createChannelStrip(const char* bundleId) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    auto strip   = std::make_unique<Strip>();
    strip->id    = impl_->nextStrip_++;
    strip->source = std::make_shared<TapSourceNode>(impl_->nextNodeId_++, bundleId);
    strip->volume = std::make_shared<VolumeNode>(impl_->nextNodeId_++);
    strip->meter  = std::make_shared<MeterNode>(impl_->nextNodeId_++);
    const StripID id = strip->id;
    impl_->strips_.emplace(id, std::move(strip));
    if (impl_->running_.load()) impl_->rebuildGraph();
    return id;
}

void AudioEngine::removeChannelStrip(StripID strip) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->strips_.erase(strip);
    if (impl_->running_.load()) impl_->rebuildGraph();
}

void AudioEngine::setVolume(StripID strip, float db) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    if (Strip* s = impl_->find(strip)) if (s->volume) s->volume->setGainDb(db);
}

void AudioEngine::setPan(StripID strip, float pan) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    if (Strip* s = impl_->find(strip)) if (s->volume) s->volume->setPan(pan);
}

void AudioEngine::setMute(StripID strip, bool muted) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    if (Strip* s = impl_->find(strip)) if (s->volume) s->volume->setMuted(muted);
}

void AudioEngine::setInputTrim(StripID strip, float db) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    if (Strip* s = impl_->find(strip)) {
        s->inputTrimDb = db;
        // TODO(Phase 2): apply the trim as a pre-effects gain (e.g. a dedicated
        // GainUtility at the head of the rack). For now it is recorded only.
    }
}

LevelSnapshot AudioEngine::getLevel(StripID strip) const {
    // Lock-free read of the meter atomics. We avoid taking mutex_ on the read
    // path; the meter node pointer is stable for the strip's lifetime, and the
    // atomics inside it are written by the audio thread. The map lookup itself
    // is not strictly lock-free, so we take the lock briefly only to resolve the
    // node, then read its atomics.
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    auto it = impl_->strips_.find(strip);
    if (it == impl_->strips_.end() || !it->second->meter) return {};
    return it->second->meter->snapshot();
}

int AudioEngine::insertBuiltinEffect(StripID strip, int slotIndex, BuiltinEffectType type) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    Strip* s = impl_->find(strip);
    if (!s) return -1;

    auto eff = createBuiltinEffect(type);
    if (!eff) return -1;

    EffectSlot slot;
    slot.handle = s->nextEffectHandle++;
    slot.node   = std::make_shared<EffectNode>(impl_->nextNodeId_++, std::move(eff));
    slot.node->prepareIfNeeded(impl_->format_);

    const int idx = std::clamp(slotIndex, 0, static_cast<int>(s->effects.size()));
    const int handle = slot.handle;
    s->effects.insert(s->effects.begin() + idx, std::move(slot));

    if (impl_->running_.load()) impl_->rebuildGraph();
    return handle;
}

void AudioEngine::setEffectParameter(StripID strip, int slot, uint32_t paramId, float value) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    Strip* s = impl_->find(strip);
    if (!s) return;
    for (auto& es : s->effects) {
        if (es.handle == slot && es.node) {
            es.node->setParameter(paramId, value);
            return;
        }
    }
}

void AudioEngine::removeEffect(StripID strip, int slot) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    Strip* s = impl_->find(strip);
    if (!s) return;
    auto& v = s->effects;
    v.erase(std::remove_if(v.begin(), v.end(),
                           [slot](const EffectSlot& es) { return es.handle == slot; }),
            v.end());
    if (impl_->running_.load()) impl_->rebuildGraph();
}

int AudioEngine::insertEffect(StripID strip, int slotIndex, const AudioComponentDescription& desc) {
    // Phase 4 — hosted AudioUnit. Stubbed: see AU/AudioUnitHost.{h,mm}.
    (void)strip; (void)slotIndex; (void)desc;
    // TODO(Phase 4): construct an EffectNode wrapping an AudioUnitHost built from
    // `desc`, insert it into the strip rack, and republish the graph.
    return -1;
}

} // namespace sonicpatch
