#pragma once
/**
 * TR909 Rim Shot circuit model.
 *
 * From TR-909 schematic (Rim Shot section):
 *  - RC differentiator (C_rs=22nF, R_rs=4.7kΩ) → sharp click
 *    τ = R_rs × C_rs ≈ 103 µs
 *  - Metallic ring oscillator: derived from coupling network R_ring/C_ring
 *    Modelled at ~1.6 kHz (measured from TR909 rim shot spectrum)
 *    Q_ring = 8 (narrow resonance for metallic character)
 *  - Schottky diode output clamp (Vf = 0.3 V)
 *
 * Schematic cross-reference: Roland TR-909 Service Notes, page 8.
 */
#include <cmath>

class TR909RimShot {
public:
    static constexpr double C_rs   = 22e-9;   // 22 nF differentiator cap
    static constexpr double R_rs   = 4700.0;  // 4.7 kΩ, τ_hp ≈ 103 µs
    static constexpr double C_ring = 100e-9;  // 100 nF ring cap
    static constexpr double R_ring = 10e3;    // 10 kΩ ring resistor

    float level = 1.0f;

    void prepare(double sampleRate);
    void trigger(float velocity, bool accent = false);
    float process();

private:
    double fs_    = 44100.0;
    double invFs_ = 1.0 / 44100.0;

    // RC HP differentiator
    double hp_a_  = 0.0;
    double hp_x1_ = 0.0, hp_y1_ = 0.0;

    // Click decaying envelope
    double clickLevel_  = 0.0;
    double clickDecay_  = 0.0;

    // Ring: phase-accumulator oscillator
    double ringPhase_  = 0.0;
    double ringEnv_    = 0.0;
    double ringDecay_  = 0.0;
    static constexpr double RING_FREQ = 1600.0; // Hz (metallic character)

    float accentGain_ = 1.0f;

    static double diodeClamp(double x) noexcept {
        const double Vf = 0.3;
        if (x >  Vf) return  Vf + 0.1 * (x - Vf);
        if (x < -Vf) return -Vf + 0.1 * (x + Vf);
        return x;
    }
};
