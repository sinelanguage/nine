#pragma once
/**
 * TR909 Bass Drum circuit model.
 *
 * Based on the Roland TR-909 schematic (Bass Drum section):
 *  - LC bridged-T oscillator: L1=35mH, C8=47µF  → ω₀=824 rad/s (131 Hz)
 *  - Q1/Q2 = 2SC1000 NPN transistors (Ebers-Moll softclip at output)
 *  - TUNE pot (VR3, 50 kΩ) shifts base frequency (~40–100 Hz range)
 *  - DECAY pot (VR1, 500 kΩ): τ_decay = VR1 × C8 (0.047 ms – 23.5 s)
 *  - Pitch envelope τ: C5×R12 = 4.7 ms (fixed by schematic)
 *  - TONE: click transient level through RC differentiator (R17, C5)
 *
 * DSP realisation:
 *  Phase-accumulator oscillator with pitch-swept instantaneous frequency.
 *  All time constants and frequencies derived from schematic component values.
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
    float tone   = 0.5f;
    float level  = 1.0f;

    void prepare(double sampleRate);
    void trigger(float velocity, bool accent = false);
    float process();

private:
    double fs_    = 44100.0;
    double invFs_ = 1.0 / 44100.0;

    // Phase-accumulator oscillator
    double phase_      = 0.0;

    // Amplitude envelope
    double ampEnv_     = 0.0;
    double ampDecay_   = 0.0;

    // Pitch envelope
    double pitchEnv_   = 0.0;
    double pitchDecay_ = 0.0;
    double fBase_      = 60.0;  // base (resting) frequency
    double fDelta_     = 100.0; // initial pitch offset above fBase

    // Click transient
    double clickLevel_ = 0.0;
    double clickDecay_ = 0.0;

    float accentGain_  = 1.0f;

    static double softclip(double x) noexcept { return std::tanh(x); }
};
