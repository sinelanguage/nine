#include "TR909Clap.h"
#include <cmath>

void TR909Clap::prepare(double sampleRate) {
    fs_    = sampleRate;
    invFs_ = 1.0 / sampleRate;
    rng_.seed(54321);

    updateFilter();

    filt_y1_ = filt_y2_ = filt_x1_ = filt_x2_ = 0.0;
    tailEnv_ = 0.0;
    active_  = false;

    for (int i = 0; i < NUM_BURSTS; ++i)
        burstEnv_[i] = 0.0;
}

void TR909Clap::updateFilter() {
    // BPF: fc from R_filt, C_filt → fc = 1/(2π R C) ≈ 2340 Hz
    // But TR909 clap resonance sits around 1.6–2.5 kHz; we use component values.
    double fc = 1.0 / (2.0 * M_PI * R_filt * C_filt); // ~2340 Hz
    double w0 = 2.0 * M_PI * fc * invFs_;
    double Q  = 4.0; // moderate resonance for metallic coloring
    double alpha = std::sin(w0) / (2.0 * Q);
    double cosw  = std::cos(w0);
    double a0inv = 1.0 / (1.0 + alpha);

    filt_b0_ =  alpha * a0inv;
    filt_b1_ =  0.0;
    filt_b2_ = -alpha * a0inv;
    filt_a1_ = -2.0 * cosw * a0inv;
    filt_a2_ =  (1.0 - alpha) * a0inv;

    // Burst decay: τ = R_burst * C_burst * 0.5 (half-burst envelope)
    double tau_b = tau_burst * 0.5;
    burstDecay_ = std::exp(-invFs_ / tau_b);

    // Burst gap: time between consecutive burst onsets = tau_burst
    burstGap_ = tau_burst * fs_; // in samples

    // Burst onset times (in samples from trigger):
    //   burst 0: 0 ms, 1: 5ms, 2: 10ms, 3: 20ms (main clap hit)
    burstSampleOffsets_[0] = 0;
    burstSampleOffsets_[1] = static_cast<int>(0.005 * fs_);
    burstSampleOffsets_[2] = static_cast<int>(0.010 * fs_);
    burstSampleOffsets_[3] = static_cast<int>(0.018 * fs_);

    // Tail decay: τ = VR1 * C_burst
    double VR1 = 1000.0 + static_cast<double>(decay) * 499000.0;
    double tau_tail = std::min(VR1 * C_burst, 2.0); // max 2s
    tailDecay_ = std::exp(-invFs_ / tau_tail);
}

void TR909Clap::trigger(float velocity, bool accent) {
    accentGain_ = accent ? 1.4f : 1.0f;
    velocity_   = velocity * accentGain_ * level;
    burstCounter_ = 0;
    active_     = true;
    tailEnv_    = 0.0;
    for (int i = 0; i < NUM_BURSTS; ++i)
        burstEnv_[i] = 0.0;

    updateFilter();
    filt_y1_ = filt_y2_ = filt_x1_ = filt_x2_ = 0.0;
}

float TR909Clap::process() {
    if (!active_) return 0.0f;

    // Fire burst envelopes at scheduled times
    for (int i = 0; i < NUM_BURSTS; ++i) {
        if (burstCounter_ == burstSampleOffsets_[i]) {
            // Burst 3 is the main, heavier hit
            burstEnv_[i] = static_cast<double>(velocity_) * (i == NUM_BURSTS - 1 ? 1.0 : 0.6);

            // Tail starts on final burst
            if (i == NUM_BURSTS - 1)
                tailEnv_ = static_cast<double>(velocity_);
        }
    }

    // Generate noise and sum burst envelopes as amplitude modulation
    double noise = noiseDist_(rng_);
    double envSum = 0.0;
    for (int i = 0; i < NUM_BURSTS; ++i) {
        envSum      += burstEnv_[i];
        burstEnv_[i] *= burstDecay_;
    }

    // Add tail
    envSum += tailEnv_ * 0.3;
    tailEnv_ *= tailDecay_;

    // Apply noise through BPF
    double x0  = noise * envSum;
    double y0  = filt_b0_ * x0 + filt_b1_ * filt_x1_ + filt_b2_ * filt_x2_
               - filt_a1_ * filt_y1_ - filt_a2_ * filt_y2_;
    filt_x2_ = filt_x1_; filt_x1_ = x0;
    filt_y2_ = filt_y1_; filt_y1_ = y0;

    ++burstCounter_;

    // Deactivate after tail has decayed to silence (approx 3 τ)
    if (burstCounter_ > 3 && envSum < 1e-6 && tailEnv_ < 1e-6)
        active_ = false;

    return static_cast<float>(std::tanh(y0 * 2.0) * 0.5);
}
