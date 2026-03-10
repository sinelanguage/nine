#include "TR909Tom.h"
#include <algorithm>
#include <cmath>

void TR909Tom::setType(TomType t) {
    type_ = t;
    switch (t) {
        case TomType::Hi:  C_val_ = C_hi;  break;
        case TomType::Mid: C_val_ = C_mid; break;
        case TomType::Lo:  C_val_ = C_lo;  break;
    }
}

void TR909Tom::prepare(double sampleRate) {
    fs_    = sampleRate;
    invFs_ = 1.0 / sampleRate;
    phase_ = 0.0;
    pitchEnv_ = ampEnv_ = clickLevel_ = 0.0;
}

void TR909Tom::trigger(float velocity, bool accent) {
    accentGain_ = accent ? 1.4f : 1.0f;
    const float v = velocity * accentGain_;

    // Natural frequency from LC components
    const double omega0 = 1.0 / std::sqrt(L_tom * C_val_);
    const double f0     = omega0 / (2.0 * M_PI);

    // TUNE scales frequency ±30%
    const double freqScale = 0.7 + static_cast<double>(tune) * 0.6;
    fBase_  = f0 * freqScale;

    // Pitch envelope: starts at fBase × (1 + tune × 3.5), decays to fBase
    fDelta_ = fBase_ * static_cast<double>(tune) * 3.5;

    // τ_pitch from C_val × R_osc (fixed by schematic)
    const double tauPitch = C_val_ * R_osc;
    pitchDecay_ = std::exp(-invFs_ / tauPitch);
    pitchEnv_   = 1.0;

    // Amplitude decay: τ = VR × C_val, VR ∈ [1kΩ, 500kΩ]
    const double VR = 1000.0 + static_cast<double>(decay) * 499000.0;
    const double tauAmp = std::min(VR * C_val_, 3.0);
    ampDecay_ = std::exp(-invFs_ / tauAmp);
    ampEnv_   = static_cast<double>(v * level);

    // Click (~1ms attack transient)
    clickDecay_ = std::exp(-invFs_ / 0.001);
    clickLevel_ = static_cast<double>(v * level) * 0.25;

    phase_ = 0.0;
}

float TR909Tom::process() {
    // Instantaneous frequency with pitch envelope
    const double fNow = fBase_ + fDelta_ * pitchEnv_;
    pitchEnv_ *= pitchDecay_;

    phase_ += 2.0 * M_PI * fNow * invFs_;
    if (phase_ > M_PI) phase_ -= 2.0 * M_PI;

    const double osc = std::sin(phase_) * ampEnv_;
    ampEnv_ *= ampDecay_;

    const double click = clickLevel_;
    clickLevel_ *= clickDecay_;

    const double out = softclip((osc + click) * 2.0) * 0.5;
    return static_cast<float>(out);
}
