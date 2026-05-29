//
// EffectFactory.cpp
//
#include "EffectFactory.hpp"

#include "ParametricEQ.hpp"
#include "Compressor.hpp"
#include "Limiter.hpp"
#include "NoiseGate.hpp"
#include "Filters.hpp"
#include "GainUtility.hpp"

namespace sonicpatch {

std::unique_ptr<IEffect> createBuiltinEffect(BuiltinEffectType type) {
    switch (type) {
        case BuiltinEffectType::ParametricEQ:        return std::make_unique<ParametricEQ>();
        case BuiltinEffectType::Compressor:          return std::make_unique<Compressor>();
        case BuiltinEffectType::Limiter:             return std::make_unique<Limiter>();
        case BuiltinEffectType::NoiseGate:           return std::make_unique<NoiseGate>();
        case BuiltinEffectType::HighLowPassFilter:   return std::make_unique<Filters>();
        case BuiltinEffectType::GainUtility:         return std::make_unique<GainUtility>();
    }
    return nullptr;
}

} // namespace sonicpatch
