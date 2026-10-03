#pragma once
//
// Biquad.hpp
// A second-order IIR (biquad) filter — the reference DSP building block.
//
// Implements the transposed Direct Form II difference equation:
//
//     y[n] = b0*x[n] + s1
//     s1   = b1*x[n] - a1*y[n] + s2
//     s2   = b2*x[n] - a2*y[n]
//
// (coefficients are normalised so a0 == 1). Transposed DF-II is preferred for
// float because it has good numerical behaviour and only two state variables.
//
// Coefficients are computed with the well-known RBJ "Audio EQ Cookbook"
// formulas, which derive the analog prototype and apply the bilinear transform
// with frequency pre-warping (the tan(w0/2) term inside the cookbook's alpha and
// the cos(w0) terms). See the .cpp for the full derivation comments.
//
#include <cstdint>

namespace sonicpatch {

/// Filter response types supported by the coefficient calculators.
enum class BiquadType {
    LowPass,
    HighPass,
    Peaking,    ///< parametric peak/notch (uses gainDb + Q)
    LowShelf,   ///< uses gainDb
    HighShelf   ///< uses gainDb
};

/// Normalised biquad coefficients (a0 == 1).
struct BiquadCoeffs {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;

    /// Compute coefficients for the given response. `sampleRate`, `freq` in Hz,
    /// `q` dimensionless, `gainDb` only used by Peaking/Shelf types.
    static BiquadCoeffs design(BiquadType type,
                               double sampleRate,
                               double freq,
                               double q,
                               double gainDb = 0.0) noexcept;
};

/// Single-channel transposed-DF-II biquad. For multichannel use one per channel.
class Biquad {
public:
    Biquad() = default;

    /// Set coefficients (control thread). Does not clear state.
    void setCoeffs(const BiquadCoeffs& c) noexcept { c_ = c; }
    const BiquadCoeffs& coeffs() const noexcept { return c_; }

    /// Clear state variables (no allocation). Safe to call anytime.
    void reset() noexcept { s1_ = 0.0f; s2_ = 0.0f; }

    /// [RT] Process one sample.
    float processSample(float x) noexcept {
        const float y = c_.b0 * x + s1_;
        s1_ = c_.b1 * x - c_.a1 * y + s2_;
        s2_ = c_.b2 * x - c_.a2 * y;
        return y;
    }

    /// [RT] Process a contiguous block in place.
    void processBlock(float* data, int frames) noexcept {
        for (int i = 0; i < frames; ++i) {
            data[i] = processSample(data[i]);
        }
    }

private:
    BiquadCoeffs c_;
    float s1_ = 0.0f; ///< state z^-1
    float s2_ = 0.0f; ///< state z^-2
};

} // namespace sonicpatch
