#include "TR909BassDrum.h"
#include <cmath>

void TR909BassDrum::prepare(double sampleRate) {
    fs_    = sampleRate;
    invFs_ = 1.0 / sampleRate;
    phase_ = 0.0;
    ampEnv_ = pitchEnv_ = clickLevel_ = 0.0;
}

void TR909BassDrum::trigger(float velocity, bool accent) {
    accentGain_ = accent ? 1.4f : 1.0f;
    const float v = velocity * accentGain_;

    // --- Derive parameters from component values ---
    // TUNE maps to f_base: 40–100 Hz (ω₀/2π = 131 Hz is the reference)
    fBase_  = 40.0 + static_cast<double>(tune) * 60.0;

    // Pitch sweep: starts at fBase * ratio, decays to fBase
    // Ratio from transistor switching: ~5× at max tune
    fDelta_ = fBase_ * (1.0 + static_cast<double>(tune) * 3.5) - fBase_;

    // τ_pitch from C5 × R12 (fixed schematic values, TUNE shifts slightly)
    const double tauPitch = C5 * R12 * (0.5 + static_cast<double>(tune) * 0.5);
    pitchDecay_ = std::exp(-invFs_ / tauPitch);
    pitchEnv_   = 1.0;

    // τ_decay from VR1 × C8; VR1 ∈ [1kΩ, 500kΩ]
    const double VR1 = 1000.0 + static_cast<double>(decay) * 499000.0;
    const double tauAmp = std::min(VR1 * C8, 4.0);
    ampDecay_ = std::exp(-invFs_ / tauAmp);
    ampEnv_   = static_cast<double>(v * level);

    // Click: RC differentiator from R17, C5 (τ = R17×C5 ≈ 2.2ms)
    const double tauClick = R17 * C5 * (0.1 + static_cast<double>(tone) * 0.9);
    clickDecay_ = std::exp(-invFs_ / tauClick);
    clickLevel_ = static_cast<double>(v * tone * level) * 0.8;

    // Reset oscillator phase to start of cycle
    phase_ = 0.0;
}

float TR909BassDrum::process() {
    // Instantaneous frequency with pitch envelope
    const double fNow = fBase_ + fDelta_ * pitchEnv_;
    pitchEnv_ *= pitchDecay_;

    // Phase accumulator (avoids IIR initialisation artifacts)
    phase_ += 2.0 * M_PI * fNow * invFs_;
    if (phase_ > M_PI) phase_ -= 2.0 * M_PI;

    // Oscillator amplitude × envelope
    const double osc = std::sin(phase_) * ampEnv_;
    ampEnv_ *= ampDecay_;

    // Click transient
    const double click = clickLevel_;
    clickLevel_ *= clickDecay_;

    // Mix and saturate (transistor output stage model)
    const double out = softclip((osc + click * static_cast<double>(tone)) * 1.5) * 0.67;
    return static_cast<float>(out);
}
