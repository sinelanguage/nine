#pragma once
/**
 * TR909 Bass Drum circuit model.
 *
 * Based on the Roland TR-909 schematic (Bass Drum section):
 *  - LC bridged-T oscillator: L1=35mH, C8=47µF  → ω₀=824 rad/s (131 Hz)
 *  - Q1/Q2 = 2SC1000 NPN transistors (Ebers-Moll softclip at output)
 *  - TUNE pot (VR3, 50 kΩ) shifts the bridged-T/LC body frequency
 *  - DECAY pot (VR1, 500 kΩ): effective τ from VR1 × C8 under circuit loading
 *  - ATTACK pot controls the trigger pulse / beater transient strength
 *  - LEVEL pot sets the final output level
 *
 * DSP realisation:
 *  Damped two-pole resonator excited by a short trigger pulse and shaped by
 *  schematic RC time constants. A short pitch sweep from C5 × R12 is retained,
 *  but the audible front-end impact comes from a differentiated beater transient
 *  rather than a separate tone control.
 */
#include <cmath>

class TR909BassDrum {
public:
    // Component values from TR-909 schematic
    static constexpr double L1  = 35e-3;   // 35 mH
    static constexpr double C8  = 47e-6;   // 47 µF
    static constexpr double R17 = 2200.0;  // 2.2 kΩ
    static constexpr double C5  = 1e-6;    // 1 µF
    static constexpr double R12 = 4700.0;  // 4.7 kΩ
    // Natural frequency: ω₀ = 1/√(L1·C8)
    // f₀ ≈ 131 Hz; scaled by TUNE pot to ~40–100 Hz operating range

    float tune   = 0.5f;
    float decay  = 0.5f;
    float attack = 0.5f;
    float level  = 1.0f;

    void prepare(double sampleRate);
    void trigger(float velocity, bool accent = false);
    float process();

private:
    double fs_    = 44100.0;
    double invFs_ = 1.0 / 44100.0;

    double bodyZ1_       = 0.0;
    double bodyZ2_       = 0.0;
    double bodyDecay_    = 0.0;
    double exciteEnv_    = 0.0;
    double exciteDecay_  = 0.0;
    double exciteState_  = 0.0;
    double pitchEnv_     = 0.0;
    double pitchDecay_   = 0.0;
    double beaterEnv_    = 0.0;
    double beaterDecay_  = 0.0;
    double beaterState_  = 0.0;
    double fBase_        = 60.0;
    double fSweepRatio_  = 1.9;

    float accentGain_  = 1.0f;

    static double softclip(double x) noexcept { return std::tanh(x); }
};
