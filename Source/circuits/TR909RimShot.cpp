#include "TR909RimShot.h"
#include <cmath>

void TR909RimShot::prepare(double sampleRate) {
    fs_    = sampleRate;
    invFs_ = 1.0 / sampleRate;

    // HP coefficient: τ = R_rs × C_rs ≈ 103 µs
    const double tau_hp = R_rs * C_rs;
    hp_a_ = tau_hp / (tau_hp + invFs_);

    // Click envelope decay: τ × 10 for audibility
    clickDecay_ = std::exp(-invFs_ / (tau_hp * 10.0));

    // Ring oscillator decay: τ = R_ring × C_ring × 20 (stretch for audibility)
    ringDecay_ = std::exp(-invFs_ / (R_ring * C_ring * 20.0));

    hp_x1_ = hp_y1_ = 0.0;
    clickLevel_ = ringEnv_ = ringPhase_ = 0.0;
}

void TR909RimShot::trigger(float velocity, bool accent) {
    accentGain_ = accent ? 1.4f : 1.0f;
    const float v = velocity * accentGain_ * level;

    clickLevel_ = static_cast<double>(v);
    ringEnv_    = static_cast<double>(v) * 0.8;
    ringPhase_  = 0.0;
    hp_x1_      = hp_y1_ = 0.0;
}

float TR909RimShot::process() {
    // --- Click: decaying exponential through HP differentiator ---
    const double clickIn = clickLevel_;
    clickLevel_ *= clickDecay_;

    // RC HP filter: y[n] = hp_a × (y[n-1] + x[n] - x[n-1])
    const double hp_y = hp_a_ * (hp_y1_ + clickIn - hp_x1_);
    hp_x1_ = clickIn;
    hp_y1_ = hp_y;

    // --- Ring oscillator (phase accumulator) ---
    ringPhase_ += 2.0 * M_PI * RING_FREQ * invFs_;
    if (ringPhase_ > M_PI) ringPhase_ -= 2.0 * M_PI;
    const double ring = std::sin(ringPhase_) * ringEnv_;
    ringEnv_ *= ringDecay_;

    // --- Mix and clamp (Schottky diode output limiter) ---
    const double out = diodeClamp(hp_y * 2.0 + ring * 0.5) * 0.8;
    return static_cast<float>(out);
}
