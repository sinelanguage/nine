#pragma once
/**
 * NineProcessor – Steinberg VST3 AudioEffect implementation.
 *
 * Implements Steinberg::Vst::AudioEffect which provides both IComponent
 * and IAudioProcessor in a single class.  No JUCE dependency.
 *
 * Responsibilities:
 *  - Declares 8 stereo output buses + 1 MIDI event input.
 *  - Routes MIDI note-on events (at the correct sample offset) to the
 *    seven TR909 circuit-model voice objects.
 *  - Applies per-block IParameterChanges to the voice parameters.
 *  - Writes audio to up to 8 stereo output buses (main mix + 7 individual).
 *  - Serialises/deserialises its state via IBStream.
 */

#pragma once
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "NineIDs.h"
#include "circuits/TR909BassDrum.h"
#include "circuits/TR909SnareDrum.h"
#include "circuits/TR909Tom.h"
#include "circuits/TR909RimShot.h"
#include "circuits/TR909Clap.h"

class NineProcessor : public Steinberg::Vst::AudioEffect
{
public:
    static const Steinberg::FUID uid;

    NineProcessor();
    ~NineProcessor() override = default;

    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IAudioProcessor*>(new NineProcessor());
    }

    // IComponent
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;

    // IAudioProcessor
    Steinberg::tresult PLUGIN_API setBusArrangements(
        Steinberg::Vst::SpeakerArrangement* inputs,  Steinberg::int32 numIns,
        Steinberg::Vst::SpeakerArrangement* outputs, Steinberg::int32 numOuts) override;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 symbolicSampleSize) override;
    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& setup) override;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) override;

    // IComponent state
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) override;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) override;

private:
    // TR909 circuit-model voice instances (pure C++, no JUCE)
    TR909BassDrum  bd_;
    TR909SnareDrum sd_;
    TR909Tom       hiTom_, midTom_, loTom_;
    TR909RimShot   rs_;
    TR909Clap      cp_;

    // Current normalised parameter values (0–1), mirroring the controller.
    // Index = NineParamID enum value.
    float params_[kNumParams] = {};

    // Voice routing
    enum VoiceID { BD = 0, SD = 1, HI = 2, MID = 3, LO = 4, RS = 5, CP = 6, VOICE_COUNT = 7 };
    // Output bus index for each voice (bus 0 = main mix, buses 1–7 = individual)
    static constexpr int VOICE_BUS[VOICE_COUNT] = { 1, 2, 3, 4, 5, 6, 7 };

    void applyParam(Steinberg::Vst::ParamID pid) noexcept;
    void applyAllParams() noexcept;
    void triggerVoice(int voice, float velocity, bool accent) noexcept;
    static int midiNoteToVoice(int note) noexcept;
};
