#include "TR909BassDrum.h"
#include <cmath>

namespace {
constexpr double kTwoPi = 6.28318530717958647692;

double expDecay(double tauSeconds, double samplePeriod) noexcept
{
    return std::exp(-samplePeriod / std::max(tauSeconds, 1.0e-6));
}

double onePoleCoeff(double cutoffHz, double sampleRate) noexcept
{
    return 1.0 - std::exp(-kTwoPi * std::max(cutoffHz, 1.0) / sampleRate);
}

double parallelResistance(double a, double b) noexcept
{
    const double sum = a + b;
    return (sum <= 1.0e-12) ? 0.0 : ((a * b) / sum);
}
} // namespace

void TR909BassDrum::prepare(double sampleRate)
{
    fs_ = sampleRate;
    invFs_ = 1.0 / sampleRate;

    bodyZ1_ = bodyZ2_ = 0.0;
    bodyExciteEnv_ = bodyExciteState_ = 0.0;
    pitchFastEnv_ = pitchSlowEnv_ = 0.0;
    transientEnv_ = transientState_ = transientToneState_ = 0.0;
    outputMemory_ = 0.0;
    accentAmount_ = 0.0;
    derivedDirty_ = true;
}

void TR909BassDrum::syncParameters() noexcept
{
    if (tune != cachedTune_ || decay != cachedDecay_ || attack != cachedAttack_ || level != cachedLevel_)
    {
        cachedTune_ = tune;
        cachedDecay_ = decay;
        cachedAttack_ = attack;
        cachedLevel_ = level;
        derivedDirty_ = true;
    }

    if (derivedDirty_)
        updateDerivedParameters();
}

void TR909BassDrum::updateDerivedParameters() noexcept
{
    derivedDirty_ = false;

    const double tuneNorm = std::pow(clamp01(tune), calibration_.tunePotTaper);
    const double decayNorm = std::pow(clamp01(decay), calibration_.decayPotTaper);
    const double attackNorm = std::pow(clamp01(attack), calibration_.attackPotTaper);
    const double levelNorm = clamp01(level);

    // Authentic / circuit-informed: LC tank base frequency from L1 and C8.
    const double lcHz = 1.0 / (kTwoPi * std::sqrt(L1 * C8));

    // Inferred: tune pot is mapped as a perceptual frequency control rather than raw ohms.
    const double tuneScale = calibration_.tuneMinScale *
        std::pow(calibration_.tuneMaxScale / calibration_.tuneMinScale, tuneNorm);
    derived_.baseHz = lcHz * tuneScale;

    // Circuit-informed: decay control is dominated by the VR1/C8 discharge path under loading.
    const double decayOhms = calibration_.decayPotMinOhms + decayNorm * calibration_.decayPotRangeOhms;
    const double effectiveR = parallelResistance(decayOhms, calibration_.bodyLoadOhms);
    const double tauBody = std::clamp(effectiveR * C8,
                                      calibration_.minBodyTauSeconds,
                                      calibration_.maxBodyTauSeconds);
    derived_.bodyPoleRadius = expDecay(tauBody, invFs_);
    derived_.bodyPoleRadiusSquared = derived_.bodyPoleRadius * derived_.bodyPoleRadius;

    // Authentic / circuit-informed anchor: R17*C5 sets the short trigger/excitation timing.
    const double exciteTau = R17 * C5 * (calibration_.exciteTauScale + calibration_.exciteTauRange * attackNorm);
    derived_.bodyExciteDecay = expDecay(exciteTau, invFs_);
    derived_.bodyExciteAmount = calibration_.exciteAmountMin + calibration_.exciteAmountRange * attackNorm;

    // Inferred from topology: attack path is brighter and shorter than the body excitation path.
    const double transientTau = R17 * C5 * (calibration_.transientTauScale + calibration_.transientTauRange * attackNorm);
    derived_.transientDecay = expDecay(transientTau, invFs_);
    derived_.transientAmount = calibration_.transientAmountMin + calibration_.transientAmountRange * attackNorm;
    derived_.transientToneCoeff = onePoleCoeff(calibration_.transientToneMinHz +
                                                   calibration_.transientToneRangeHz * attackNorm,
                                               fs_);

    // Authentic / circuit-informed anchor: R12*C5 shapes the downward pitch sweep.
    const double pitchTau = R12 * C5;
    derived_.pitchFastDecay = expDecay(pitchTau * calibration_.pitchFastTauScale, invFs_);
    derived_.pitchSlowDecay = expDecay(pitchTau * calibration_.pitchSlowTauScale, invFs_);
    derived_.pitchFastDepth = calibration_.pitchFastDepthMin + calibration_.pitchFastDepthRange * attackNorm;
    derived_.pitchSlowDepth = calibration_.pitchSlowDepth * (0.9 + 0.2 * attackNorm);

    // Estimated / perceptual mapping: level behaves more like output gain than linear voltage.
    derived_.levelGain = std::pow(levelNorm, calibration_.levelPotTaper);
    derived_.outputMemoryCoeff = onePoleCoeff(calibration_.outputMemoryHz, fs_);
}

void TR909BassDrum::trigger(float velocity, bool accent)
{
    syncParameters();

    const double vel = clamp01(velocity);

    // Inferred: accent is a trigger-time control that changes level and drive, not only amplitude.
    accentAmount_ = accent ? 1.0 : 0.0;
    const double strikeLevel = vel * derived_.levelGain * (1.0 + calibration_.accentLevelBoost * accentAmount_);

    bodyExciteEnv_ = strikeLevel * derived_.bodyExciteAmount;
    bodyExciteState_ = 0.0;

    transientEnv_ = strikeLevel * derived_.transientAmount *
        (1.0 + calibration_.accentTransientBoost * accentAmount_);
    transientState_ = 0.0;
    transientToneState_ = 0.0;

    pitchFastEnv_ = 1.0 + calibration_.accentPitchBoost * accentAmount_;
    pitchSlowEnv_ = 1.0;

    // Authentic / circuit-informed simplification: the analog trigger effectively re-primes the resonant path.
    bodyZ1_ = 0.0;
    bodyZ2_ = 0.0;
    outputMemory_ = 0.0;
}

double TR909BassDrum::applyOutputStage(double x) noexcept
{
    // Estimated / calibration-needed: output-stage memory approximates transistor/op-amp slew/compression.
    outputMemory_ += derived_.outputMemoryCoeff * (x - outputMemory_);
    const double stageInput = x + 0.35 * outputMemory_;

    // Inferred: accent increases both drive and bias, yielding a brighter/denser hit.
    const double drive = calibration_.outputDrive * (1.0 + calibration_.accentDriveBoost * accentAmount_);
    const double bias = calibration_.outputBias + calibration_.accentBiasBoost * accentAmount_;
    const double posDrive = drive * (1.0 + calibration_.outputAsymmetry);
    const double negDrive = drive * (1.0 - 0.5 * calibration_.outputAsymmetry);

    const double shaped = 0.5 * (std::tanh((stageInput + bias) * posDrive) +
                                 std::tanh((stageInput - bias) * negDrive));
    const double dc = 0.5 * (std::tanh(bias * posDrive) + std::tanh(-bias * negDrive));
    return (shaped - dc) * calibration_.outputGain;
}

float TR909BassDrum::process()
{
    syncParameters();

    const double fNow = derived_.baseHz *
        (1.0 + derived_.pitchFastDepth * pitchFastEnv_ + derived_.pitchSlowDepth * pitchSlowEnv_);
    pitchFastEnv_ *= derived_.pitchFastDecay;
    pitchSlowEnv_ *= derived_.pitchSlowDecay;

    const double omega = kTwoPi * std::min(fNow, fs_ * 0.45) * invFs_;

    const double bodyExcite = bodyExciteEnv_ - bodyExciteState_;
    bodyExciteState_ = bodyExciteEnv_;
    bodyExciteEnv_ *= derived_.bodyExciteDecay;

    // Inferred from the trigger network: a differentiated/bright transient feeds both the click and body attack.
    const double transientPulse = transientEnv_ - transientState_;
    transientState_ = transientEnv_;
    transientEnv_ *= derived_.transientDecay;
    transientToneState_ += derived_.transientToneCoeff * (transientPulse - transientToneState_);
    const double transientClick = transientPulse - transientToneState_;

    const double a1 = 2.0 * derived_.bodyPoleRadius * std::cos(omega);
    const double body = bodyExcite +
        transientClick * calibration_.transientToBodyCoupling +
        a1 * bodyZ1_ -
        derived_.bodyPoleRadiusSquared * bodyZ2_;
    bodyZ2_ = snapToZero(bodyZ1_);
    bodyZ1_ = snapToZero(body);

    const double shapedBody = softclip(body * calibration_.bodyDrive);
    const double shapedTransient = softclip(transientClick * calibration_.transientDrive);
    const double out = applyOutputStage(shapedBody + shapedTransient);

    return static_cast<float>(snapToZero(out));
}
