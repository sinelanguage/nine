#pragma once
/**
 * TR909 Bass Drum circuit model.
 *
 * Based on the Roland TR-909 schematic (Bass Drum section):
 *  - LC bridged-T oscillator: L1=35mH, C8=47µF  → ω₀=824 rad/s (131 Hz)
 *  - Q1/Q2 = 2SC1000 NPN transistors (Ebers-Moll softclip at output)
 *  - TUNE pot (VR3, 50 kΩ) shifts the bridged-T/LC body frequency
 *  - DECAY pot (VR1, 500 kΩ): effective τ from VR1 × C8 under circuit loading
 *  - ATTACK pot controls the trigger pulse / beater transient strength
 *  - LEVEL pot sets the final output level
 *
 * DSP realisation:
 *  Damped two-pole resonator excited by a short trigger pulse and shaped by
 *  schematic RC time constants. A short pitch sweep from C5 × R12 is retained,
 *  but the audible front-end impact comes from a differentiated beater transient
 *  rather than a separate tone control.
 */
#include <algorithm>
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
    float attack = 0.5f;
    float level  = 1.0f;

    void prepare(double sampleRate);
    void trigger(float velocity, bool accent = false);
    float process();

private:
    struct Calibration {
        // Circuit-informed calibration hooks: nominal electrical ranges from the schematic.
        double decayPotMinOhms = 1000.0;
        double decayPotRangeOhms = 499000.0;
        double bodyLoadOhms = 18000.0;

        // Inferred calibration hooks: likely knob law and musically useful electrical span.
        double tunePotTaper = 0.82;
        double decayPotTaper = 1.35;
        double attackPotTaper = 1.15;
        double levelPotTaper = 2.0;
        double tuneMinScale = 0.34;
        double tuneMaxScale = 0.74;
        double minBodyTauSeconds = 0.030;
        double maxBodyTauSeconds = 0.950;

        // Circuit-informed RC anchors with inferred scaling for audible behaviour.
        double exciteTauScale = 0.30;
        double exciteTauRange = 0.75;
        double exciteAmountMin = 0.18;
        double exciteAmountRange = 0.68;
        double transientTauScale = 0.08;
        double transientTauRange = 0.22;
        double transientAmountMin = 0.05;
        double transientAmountRange = 0.80;
        double transientToBodyCoupling = 0.20;
        double transientToneMinHz = 2200.0;
        double transientToneRangeHz = 4200.0;
        double pitchFastTauScale = 0.38;
        double pitchSlowTauScale = 2.10;
        double pitchFastDepthMin = 1.10;
        double pitchFastDepthRange = 0.95;
        double pitchSlowDepth = 0.22;

        // Estimated but calibration-ready: accent and output-stage interactions.
        double accentLevelBoost = 0.28;
        double accentTransientBoost = 0.45;
        double accentPitchBoost = 0.18;
        double accentDriveBoost = 0.35;
        double accentBiasBoost = 0.03;
        double bodyDrive = 1.75;
        double transientDrive = 5.25;
        double outputDrive = 1.25;
        double outputBias = 0.035;
        double outputAsymmetry = 0.22;
        double outputMemoryHz = 1800.0;
        double outputGain = 0.62;
    };

    struct DerivedParameters {
        double baseHz = 60.0;
        double bodyPoleRadius = 0.999;
        double bodyPoleRadiusSquared = 0.998;
        double bodyExciteDecay = 0.99;
        double bodyExciteAmount = 0.5;
        double transientDecay = 0.99;
        double transientAmount = 0.4;
        double transientToneCoeff = 0.1;
        double pitchFastDecay = 0.99;
        double pitchSlowDecay = 0.999;
        double pitchFastDepth = 1.6;
        double pitchSlowDepth = 0.2;
        double levelGain = 1.0;
        double outputMemoryCoeff = 0.1;
    };

    double fs_    = 44100.0;
    double invFs_ = 1.0 / 44100.0;
    Calibration calibration_{};
    DerivedParameters derived_{};

    double bodyZ1_             = 0.0;
    double bodyZ2_             = 0.0;
    double bodyExciteEnv_      = 0.0;
    double bodyExciteState_    = 0.0;
    double pitchFastEnv_       = 0.0;
    double pitchSlowEnv_       = 0.0;
    double transientEnv_       = 0.0;
    double transientState_     = 0.0;
    double transientToneState_ = 0.0;
    double outputMemory_       = 0.0;

    double accentAmount_ = 0.0;
    float cachedTune_ = -1.0f;
    float cachedDecay_ = -1.0f;
    float cachedAttack_ = -1.0f;
    float cachedLevel_ = -1.0f;
    bool derivedDirty_ = true;

    void syncParameters() noexcept;
    void updateDerivedParameters() noexcept;
    double applyOutputStage(double x) noexcept;

    static double clamp01(double x) noexcept { return std::clamp(x, 0.0, 1.0); }
    static double softclip(double x) noexcept { return std::tanh(x); }
    static double snapToZero(double x) noexcept { return (std::abs(x) < 1.0e-15) ? 0.0 : x; }
};
