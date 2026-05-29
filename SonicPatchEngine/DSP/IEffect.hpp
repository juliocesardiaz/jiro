#pragma once
//
// IEffect.hpp
// Interface implemented by every builtin DSP effect (and, via an adapter, by a
// hosted AudioUnit). EffectNode owns an IEffect and drives it in place.
//
// Lifecycle: prepare() (non-RT, may allocate) is called whenever the format
// changes; process() (RT) must allocate nothing. setParameter() is called from
// the control thread; concrete effects must publish parameter changes to the
// audio thread in an RT-safe way (e.g. recompute coefficients and store them in
// atomics or double-buffered state). reset() clears history without realloc.
//
#include "../Public/EngineTypes.hpp"

#include <cstdint>

namespace sonicpatch {

class IEffect {
public:
    virtual ~IEffect() = default;

    /// Non-RT: allocate/prepare for the given format. Called before the effect
    /// goes live and on every format change.
    virtual void prepare(const AudioFormat& fmt) = 0;

    /// Non-RT (or RT-safe if it only clears): clear internal history/state.
    virtual void reset() = 0;

    /// [RT] In-place processing. `io` is an array of `channels` pointers, each
    /// pointing to `frames` float samples (non-interleaved). Must not allocate,
    /// lock, block, or throw.
    virtual void process(float** io, int channels, int frames) = 0;

    /// Set an effect-specific parameter. `id` enumerations are defined per
    /// effect (see each effect header). Control thread.
    virtual void setParameter(uint32_t id, float value) = 0;

    /// Human-readable effect name (stable, for diagnostics/UI).
    virtual const char* name() const = 0;
};

} // namespace sonicpatch
