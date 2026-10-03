//
// Biquad.cpp
// RBJ Audio-EQ-Cookbook coefficient calculators.
//
// Reference: Robert Bristow-Johnson, "Cookbook formulae for audio EQ biquad
// filter coefficients." The cookbook derives each filter from its analog
// prototype and applies the bilinear transform with frequency pre-warping baked
// into the intermediate quantities:
//
//     w0    = 2*pi*freq/sampleRate            (digital centre frequency)
//     cosw0 = cos(w0)
//     sinw0 = sin(w0)
//     alpha = sin(w0) / (2*Q)                 (bandwidth term)
//
// The tan()-style pre-warping of the bilinear transform is implicit in the use
// of w0/alpha: the cookbook formulas already map the analog cutoff to the
// correct digital bin, so DC and Nyquist behave as expected.
//
#include "Biquad.hpp"

#include <cmath>

namespace sonicpatch {

namespace {
constexpr double kPi = 3.14159265358979323846;

// Clamp frequency to a sane open interval so cos/sin and the resulting
// coefficients stay well-defined (avoid f==0 and f>=Nyquist).
double clampFreq(double freq, double sampleRate) {
    const double nyq = sampleRate * 0.5;
    if (freq < 1.0) return 1.0;
    if (freq > nyq - 1.0) return nyq - 1.0;
    return freq;
}

double clampQ(double q) {
    return q < 1e-4 ? 1e-4 : q;
}
} // namespace

BiquadCoeffs BiquadCoeffs::design(BiquadType type,
                                  double sampleRate,
                                  double freq,
                                  double q,
                                  double gainDb) noexcept {
    BiquadCoeffs out;

    freq = clampFreq(freq, sampleRate);
    q    = clampQ(q);

    const double w0    = 2.0 * kPi * freq / sampleRate;
    const double cosw0 = std::cos(w0);
    const double sinw0 = std::sin(w0);
    const double alpha = sinw0 / (2.0 * q);

    // A = sqrt(10^(gainDb/20)) for shelves/peaking (the cookbook's "A").
    const double A = std::pow(10.0, gainDb / 40.0);

    // Unnormalised coefficients (b*, a*); we divide through by a0 at the end.
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a0 = 1.0, a1 = 0.0, a2 = 0.0;

    switch (type) {
        case BiquadType::LowPass: {
            // H(s) = 1 / (s^2 + s/Q + 1)
            b0 = (1.0 - cosw0) * 0.5;
            b1 =  1.0 - cosw0;
            b2 = (1.0 - cosw0) * 0.5;
            a0 =  1.0 + alpha;
            a1 = -2.0 * cosw0;
            a2 =  1.0 - alpha;
            break;
        }
        case BiquadType::HighPass: {
            // H(s) = s^2 / (s^2 + s/Q + 1)
            b0 =  (1.0 + cosw0) * 0.5;
            b1 = -(1.0 + cosw0);
            b2 =  (1.0 + cosw0) * 0.5;
            a0 =   1.0 + alpha;
            a1 =  -2.0 * cosw0;
            a2 =   1.0 - alpha;
            break;
        }
        case BiquadType::Peaking: {
            // H(s) = (s^2 + s*(A/Q) + 1) / (s^2 + s/(A*Q) + 1)
            b0 = 1.0 + alpha * A;
            b1 = -2.0 * cosw0;
            b2 = 1.0 - alpha * A;
            a0 = 1.0 + alpha / A;
            a1 = -2.0 * cosw0;
            a2 = 1.0 - alpha / A;
            break;
        }
        case BiquadType::LowShelf: {
            const double sqrtA2alpha = 2.0 * std::sqrt(A) * alpha;
            b0 =      A * ((A + 1.0) - (A - 1.0) * cosw0 + sqrtA2alpha);
            b1 =  2.0 * A * ((A - 1.0) - (A + 1.0) * cosw0);
            b2 =      A * ((A + 1.0) - (A - 1.0) * cosw0 - sqrtA2alpha);
            a0 =          (A + 1.0) + (A - 1.0) * cosw0 + sqrtA2alpha;
            a1 = -2.0 *   ((A - 1.0) + (A + 1.0) * cosw0);
            a2 =          (A + 1.0) + (A - 1.0) * cosw0 - sqrtA2alpha;
            break;
        }
        case BiquadType::HighShelf: {
            const double sqrtA2alpha = 2.0 * std::sqrt(A) * alpha;
            b0 =      A * ((A + 1.0) + (A - 1.0) * cosw0 + sqrtA2alpha);
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosw0);
            b2 =      A * ((A + 1.0) + (A - 1.0) * cosw0 - sqrtA2alpha);
            a0 =          (A + 1.0) - (A - 1.0) * cosw0 + sqrtA2alpha;
            a1 =  2.0 *   ((A - 1.0) - (A + 1.0) * cosw0);
            a2 =          (A + 1.0) - (A - 1.0) * cosw0 - sqrtA2alpha;
            break;
        }
    }

    // Normalise so a0 == 1.
    const double inv = (a0 != 0.0) ? (1.0 / a0) : 1.0;
    out.b0 = static_cast<float>(b0 * inv);
    out.b1 = static_cast<float>(b1 * inv);
    out.b2 = static_cast<float>(b2 * inv);
    out.a1 = static_cast<float>(a1 * inv);
    out.a2 = static_cast<float>(a2 * inv);
    return out;
}

} // namespace sonicpatch
