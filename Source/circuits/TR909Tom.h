#pragma once
/**
 * TR909 Tom circuit model (High, Mid, Low Tom).
 *
 * All three toms share identical topology, differing only in C value:
 *   Hi-Tom:  L=35mH, C=4.7µF  → f₀ ≈ 392 Hz
 *   Mid-Tom: L=35mH, C=10µF   → f₀ ≈ 269 Hz
 *   Lo-Tom:  L=35mH, C=22µF   → f₀ ≈ 181 Hz
 *
 * TUNE (VR, 50kΩ) scales f₀ by ±30%.
 * DECAY (VR, 500kΩ): τ = VR × C (same cap as resonator).
 * Pitch envelope τ from C × R_osc (2.2kΩ).
 */
#include <cmath>

enum class TomType { Hi, Mid, Lo };

class TR909Tom {
public:
    static constexpr double L_tom = 35e-3;    // 35 mH (all toms)
    static constexpr double C_hi  = 4.7e-6;   // Hi Tom
    static constexpr double C_mid = 10e-6;    // Mid Tom
    static constexpr double C_lo  = 22e-6;    // Lo Tom
    static constexpr double R_osc = 2200.0;   // 2.2 kΩ

    float tune   = 0.5f;
    float decay  = 0.5f;
    float level  = 1.0f;

    void setType(TomType t);
    void prepare(double sampleRate);
    void trigger(float velocity, bool accent = false);
    float process();

private:
    TomType type_  = TomType::Hi;
    double  C_val_ = C_hi;

    double fs_    = 44100.0;
    double invFs_ = 1.0 / 44100.0;

    // Phase accumulator
    double phase_      = 0.0;
    double fBase_      = 300.0;
    double fDelta_     = 0.0;

    // Pitch envelope
    double pitchEnv_   = 0.0;
    double pitchDecay_ = 0.0;

    // Amplitude envelope
    double ampEnv_     = 0.0;
    double ampDecay_   = 0.0;

    // Click
    double clickLevel_ = 0.0;
    double clickDecay_ = 0.0;

    float accentGain_  = 1.0f;

    static double softclip(double x) noexcept { return std::tanh(x); }
};
