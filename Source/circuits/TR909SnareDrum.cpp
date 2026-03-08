#include "TR909SnareDrum.h"
#include <cmath>

void TR909SnareDrum::prepare(double sampleRate) {
    fs_    = sampleRate;
    invFs_ = 1.0 / sampleRate;
    phase1_ = phase2_ = 0.0;
    nhp_x1_ = nhp_y1_ = nlp_y1_ = 0.0;
    ampEnv_ = noiseEnv_ = clickLevel_ = 0.0;
    rng_.seed(12345);
}

void TR909SnareDrum::trigger(float velocity, bool accent) {
    accentGain_ = accent ? 1.4f : 1.0f;
    const float v = velocity * accentGain_;

    // Frequency from LC values, scaled by TUNE ±30%
    const double freqScale = 0.7 + static_cast<double>(tune) * 0.6;
    f1_ = (1.0 / (2.0 * M_PI * std::sqrt(L3 * C14))) * freqScale;
    f2_ = (1.0 / (2.0 * M_PI * std::sqrt(L4 * C16))) * freqScale;

    // Body amplitude decay: τ = VR1 × C14
    const double VR1 = 1000.0 + static_cast<double>(decay) * 499000.0;
    const double tauAmp = std::min(VR1 * C14, 2.0);
    ampDecay_ = std::exp(-invFs_ / tauAmp);
    ampEnv_   = static_cast<double>(v * level);

    // Noise (snare rattle) decay
    const double tauNoise = 0.05 + static_cast<double>(snappy) * 0.25;
    noiseDecay_ = std::exp(-invFs_ / tauNoise);
    noiseEnv_   = static_cast<double>(v * level);

    // Click decay: ~1 ms (attack transient)
    clickDecay_ = std::exp(-invFs_ / 0.001);
    clickLevel_ = static_cast<double>(v * level) * 0.4;

    // Reset oscillator phases
    phase1_ = phase2_ = 0.0;
}

float TR909SnareDrum::process() {
    // --- Body oscillators (phase accumulator) ---
    phase1_ += 2.0 * M_PI * f1_ * invFs_;
    if (phase1_ > M_PI) phase1_ -= 2.0 * M_PI;
    phase2_ += 2.0 * M_PI * f2_ * invFs_;
    if (phase2_ > M_PI) phase2_ -= 2.0 * M_PI;

    const double body = (std::sin(phase1_) * 0.55 + std::sin(phase2_) * 0.45) * ampEnv_;
    ampEnv_ *= ampDecay_;

    // --- Noise (snare rattle) ---
    const double noise = noiseDist_(rng_);

    // HP filter (fc ≈ 2 kHz) – removes low rumble
    const double hp_rc = 1.0 / (2.0 * M_PI * 2000.0);
    const double hp_a  = hp_rc / (hp_rc + invFs_);
    const double nhp   = hp_a * (nhp_y1_ + noise - nhp_x1_);
    nhp_x1_ = noise;
    nhp_y1_ = nhp;

    // LP filter (fc ≈ 8 kHz) – removes extreme highs
    const double lp_a = invFs_ / (1.0 / (2.0 * M_PI * 8000.0) + invFs_);
    const double nlp  = nlp_y1_ + lp_a * (nhp - nlp_y1_);
    nlp_y1_ = nlp;

    const double noiseSig = nlp * noiseEnv_;
    noiseEnv_ *= noiseDecay_;

    // Click
    const double click = clickLevel_;
    clickLevel_ *= clickDecay_;

    // SNAPPY controls noise/body mix
    const double bodyMix  = 1.0 - static_cast<double>(snappy) * 0.5;
    const double noiseMix = 0.3 + static_cast<double>(snappy) * 0.7;

    const double out = body * bodyMix + noiseSig * noiseMix + click * 0.3;
    return static_cast<float>(softclip(out * 2.5) * 0.4);
}
