#pragma once
/**
 * TR909 Snare Drum circuit model.
 *
 * From TR-909 schematic (Snare Drum section):
 *  Two resonant body oscillators + filtered noise:
 *   Body 1: L3=35mH, C14=10µF  → f₁ ≈ 269 Hz
 *   Body 2: L4=35mH, C16=4.7µF → f₂ ≈ 392 Hz
 *  Noise: white noise → HP(2kHz) → LP(8kHz) for snare rattle
 *  TUNE (VR2, 50kΩ) → scales f₁, f₂ together (±30%)
 *  DECAY (VR1, 500kΩ): τ = VR1×C14
 *  SNAPPY (VR3, 50kΩ): noise-vs-body mix ratio
 */
#include <cmath>
#include <random>

class TR909SnareDrum {
public:
    // Component values from schematic
    static constexpr double L3  = 35e-3;  // 35 mH body resonator 1
    static constexpr double C14 = 10e-6;  // 10 µF → f₁ ≈ 269 Hz
    static constexpr double L4  = 35e-3;  // 35 mH body resonator 2
    static constexpr double C16 = 4.7e-6; // 4.7 µF → f₂ ≈ 392 Hz

    float tune   = 0.5f;
    float decay  = 0.5f;
    float snappy = 0.5f;
    float level  = 1.0f;

    void prepare(double sampleRate);
    void trigger(float velocity, bool accent = false);
    float process();

private:
    double fs_    = 44100.0;
    double invFs_ = 1.0 / 44100.0;

    // Body oscillator 1 (phase accumulator)
    double phase1_     = 0.0;
    double f1_         = 269.0;

    // Body oscillator 2 (phase accumulator)
    double phase2_     = 0.0;
    double f2_         = 392.0;

    // Body amplitude envelope
    double ampEnv_     = 0.0;
    double ampDecay_   = 0.0;

    // Noise filter state (HP → LP chain)
    double nhp_x1_  = 0.0, nhp_y1_ = 0.0;
    double nlp_y1_  = 0.0;

    // Noise amplitude envelope
    double noiseEnv_   = 0.0;
    double noiseDecay_ = 0.0;

    // Click
    double clickLevel_  = 0.0;
    double clickDecay_  = 0.0;

    float accentGain_ = 1.0f;

    std::mt19937 rng_;
    std::uniform_real_distribution<double> noiseDist_{ -1.0, 1.0 };

    static double softclip(double x) noexcept { return std::tanh(x); }
};
