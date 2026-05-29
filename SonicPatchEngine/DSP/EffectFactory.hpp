#pragma once
//
// EffectFactory.hpp
// Maps a BuiltinEffectType to a freshly-constructed IEffect instance.
//
#include "../Public/EngineTypes.hpp"
#include "IEffect.hpp"

#include <memory>

namespace sonicpatch {

/// Construct a builtin effect. Returns nullptr for unknown types. Non-RT
/// (allocates) — call at config-build time, never on the audio thread.
std::unique_ptr<IEffect> createBuiltinEffect(BuiltinEffectType type);

} // namespace sonicpatch
