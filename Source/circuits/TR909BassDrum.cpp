#include "TR909BassDrum.h"
#include <cmath>

void TR909BassDrum::prepare(double sampleRate) {
    fs_    = sampleRate;
    invFs_ = 1.0 / sampleRate;
    bodyZ1_ = bodyZ2_ = 0.0;
    exciteEnv_ = exciteState_ = pitchEnv_ = beaterEnv_ = beaterState_ = 0.0;
}

void TR909BassDrum::trigger(float velocity, bool accent) {
    accentGain_ = accent ? 1.4f : 1.0f;
    const double v = static_cast<double>(velocity * accentGain_ * level);
    const double tuneNorm = static_cast<double>(tune);
    const double decayNorm = static_cast<double>(decay);
    const double attackNorm = static_cast<double>(attack);

    // --- Derive parameters from component values and control laws ---
    const double lcHz = 1.0 / (2.0 * M_PI * std::sqrt(L1 * C8));
    fBase_ = lcHz * (0.36 + 0.44 * tuneNorm);
    fSweepRatio_ = 1.9 + 0.25 * (0.5 - tuneNorm);

    // Pitch sweep from the trigger charge on C5 through R12.
    const double tauPitch = C5 * R12;
    pitchDecay_ = std::exp(-invFs_ / tauPitch);
    pitchEnv_   = 1.0;

    // Loaded decay path for the resonant body.
    const double VR1 = 1000.0 + decayNorm * 499000.0;
    const double bodyLoad = 18000.0;
    const double effectiveR = (VR1 * bodyLoad) / (VR1 + bodyLoad);
    const double tauBody = std::max(0.025, effectiveR * C8);
    bodyDecay_ = std::exp(-invFs_ / tauBody);

    // Short attack excitation into the resonant network.
    const double tauExcite = R17 * C5 * (0.35 + 0.9 * attackNorm);
    exciteDecay_ = std::exp(-invFs_ / tauExcite);
    exciteEnv_ = v * (0.18 + 0.82 * attackNorm);
    exciteState_ = 0.0;

    // Differentiated beater transient for the ATTACK control.
    const double tauBeater = R17 * C5 * (0.12 + 0.55 * attackNorm);
    beaterDecay_ = std::exp(-invFs_ / tauBeater);
    beaterEnv_ = v * (0.06 + 0.74 * attackNorm);
    beaterState_ = 0.0;

    bodyZ1_ = bodyZ2_ = 0.0;
}

float TR909BassDrum::process() {
    const double fNow = fBase_ * (1.0 + fSweepRatio_ * pitchEnv_);
    pitchEnv_ *= pitchDecay_;
    const double omega = 2.0 * M_PI * std::fmin(fNow, fs_ * 0.45) * invFs_;

    const double excite = exciteEnv_ - exciteState_;
    exciteState_ = exciteEnv_;
    exciteEnv_ *= exciteDecay_;
    const double a1 = 2.0 * bodyDecay_ * std::cos(omega);
    const double a2 = bodyDecay_ * bodyDecay_;
    const double body = excite + a1 * bodyZ1_ - a2 * bodyZ2_;
    bodyZ2_ = bodyZ1_;
    bodyZ1_ = body;

    const double beaterDelta = beaterEnv_ - beaterState_;
    beaterState_ = beaterEnv_;
    beaterEnv_ *= beaterDecay_;

    const double shapedBody = softclip(body * 1.6);
    const double shapedBeater = softclip(beaterDelta * 6.0);
    const double out = softclip(shapedBody + shapedBeater) * 0.72;
    return static_cast<float>(out);
}
