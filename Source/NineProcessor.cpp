#include "NineProcessor.h"
#include "NineController.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include <cstring>
#include <algorithm>

using namespace Steinberg;
using namespace Steinberg::Vst;

// ---------------------------------------------------------------------------
// Plugin UID – defined in NineIDs.h, assigned here via the static const member
// ---------------------------------------------------------------------------
const FUID NineProcessor::uid = NineProcessorUID;

// ---------------------------------------------------------------------------
// Default parameter values (normalised 0–1)
// ---------------------------------------------------------------------------
static const float kDefaults[kNumParams] = {
    0.5f, // kBdTune
    0.5f, // kBdDecay
    0.5f, // kBdAttack
    0.8f, // kBdLevel
    0.5f, // kSdTune
    0.5f, // kSdDecay
    0.5f, // kSdSnappy
    0.8f, // kSdLevel
    0.6f, // kHiTune
    0.4f, // kHiDecay
    0.8f, // kHiLevel
    0.5f, // kMidTune
    0.5f, // kMidDecay
    0.8f, // kMidLevel
    0.4f, // kLoTune
    0.6f, // kLoDecay
    0.8f, // kLoLevel
    0.8f, // kRsLevel
    0.5f, // kCpDecay
    0.8f, // kCpLevel
};

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
NineProcessor::NineProcessor()
{
    // Tell AudioEffect which EditController class owns our parameters
    setControllerClass(NineController::uid);

    // Load defaults
    for (int i = 0; i < kNumParams; ++i)
        params_[i] = kDefaults[i];

    // Initialise tom types
    hiTom_.setType(TomType::Hi);
    midTom_.setType(TomType::Mid);
    loTom_.setType(TomType::Lo);
}

// ---------------------------------------------------------------------------
// IComponent::initialize – declare buses
// ---------------------------------------------------------------------------
tresult PLUGIN_API NineProcessor::initialize(FUnknown* context)
{
    tresult result = AudioEffect::initialize(context);
    if (result != kResultTrue)
        return result;

    // MIDI event input (1 channel)
    addEventInput(STR16("MIDI In"), 1);

    // Audio outputs – main mix first (required), then individual voices
    addAudioOutput(STR16("Main Mix"), SpeakerArr::kStereo);
    addAudioOutput(STR16("Bass Drum"), SpeakerArr::kStereo);
    addAudioOutput(STR16("Snare"),     SpeakerArr::kStereo);
    addAudioOutput(STR16("Hi Tom"),    SpeakerArr::kStereo);
    addAudioOutput(STR16("Mid Tom"),   SpeakerArr::kStereo);
    addAudioOutput(STR16("Lo Tom"),    SpeakerArr::kStereo);
    addAudioOutput(STR16("Rim Shot"),  SpeakerArr::kStereo);
    addAudioOutput(STR16("Clap"),      SpeakerArr::kStereo);

    return kResultTrue;
}

// ---------------------------------------------------------------------------
// IAudioProcessor::setBusArrangements
// ---------------------------------------------------------------------------
tresult PLUGIN_API NineProcessor::setBusArrangements(
    SpeakerArrangement* inputs,  int32 numIns,
    SpeakerArrangement* outputs, int32 numOuts)
{
    // We have no audio inputs
    if (numIns != 0)
        return kResultFalse;

    // At least 1 output (main mix), maximum 8
    if (numOuts < 1 || numOuts > 8)
        return kResultFalse;

    // All active output buses must be stereo
    for (int32 i = 0; i < numOuts; ++i)
        if (outputs[i] != SpeakerArr::kStereo)
            return kResultFalse;

    return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
}

// ---------------------------------------------------------------------------
// IAudioProcessor::canProcessSampleSize
// ---------------------------------------------------------------------------
tresult PLUGIN_API NineProcessor::canProcessSampleSize(int32 symbolicSampleSize)
{
    return (symbolicSampleSize == kSample32) ? kResultTrue : kResultFalse;
}

// ---------------------------------------------------------------------------
// IAudioProcessor::setupProcessing – called when sample rate changes
// ---------------------------------------------------------------------------
tresult PLUGIN_API NineProcessor::setupProcessing(ProcessSetup& setup)
{
    const double sr = setup.sampleRate;
    bd_.prepare(sr);
    sd_.prepare(sr);
    hiTom_.prepare(sr);
    midTom_.prepare(sr);
    loTom_.prepare(sr);
    rs_.prepare(sr);
    cp_.prepare(sr);
    applyAllParams();
    return AudioEffect::setupProcessing(setup);
}

// ---------------------------------------------------------------------------
// IAudioProcessor::process – audio + MIDI callback
// ---------------------------------------------------------------------------
tresult PLUGIN_API NineProcessor::process(ProcessData& data)
{
    // --- 1. Apply parameter changes (take last point per param per block) ---
    if (data.inputParameterChanges)
    {
        const int32 numChanged = data.inputParameterChanges->getParameterCount();
        for (int32 i = 0; i < numChanged; ++i)
        {
            IParamValueQueue* queue = data.inputParameterChanges->getParameterData(i);
            if (!queue) continue;
            const ParamID pid = queue->getParameterId();
            const int32   nPts = queue->getPointCount();
            if (nPts > 0 && pid < kNumParams)
            {
                int32      sampleOffset;
                ParamValue value;
                if (queue->getPoint(nPts - 1, sampleOffset, value) == kResultTrue)
                {
                    params_[pid] = static_cast<float>(value);
                    applyParam(pid);
                }
            }
        }
    }

    // --- 2. Collect MIDI note-on events (avoid dynamic allocation) ---
    struct NoteOn { int32 offset; int note; float vel; };
    NoteOn noteOns[64];
    int    numNoteOns = 0;

    if (data.inputEvents)
    {
        const int32 numEvents = data.inputEvents->getEventCount();
        for (int32 i = 0; i < numEvents && numNoteOns < 64; ++i)
        {
            Event ev{};
            if (data.inputEvents->getEvent(i, ev) != kResultTrue) continue;
            if (ev.type == Event::kNoteOnEvent && ev.noteOn.velocity > 0.0f)
                noteOns[numNoteOns++] = { ev.sampleOffset, ev.noteOn.pitch, ev.noteOn.velocity };
        }
        // VST3 spec guarantees events are ordered by sampleOffset; no sort needed.
    }

    // --- 3. Clear all output buffers ---
    for (int32 b = 0; b < data.numOutputs; ++b)
    {
        for (int32 c = 0; c < data.outputs[b].numChannels; ++c)
            std::memset(data.outputs[b].channelBuffers32[c], 0,
                        static_cast<size_t>(data.numSamples) * sizeof(float));
        data.outputs[b].silenceFlags = 0;
    }

    // --- 4. Render sample-by-sample ---
    int evIdx = 0;
    for (int32 s = 0; s < data.numSamples; ++s)
    {
        // Trigger voices at the exact sample where their MIDI event falls
        while (evIdx < numNoteOns && noteOns[evIdx].offset <= s)
        {
            const int voice = midiNoteToVoice(noteOns[evIdx].note);
            if (voice >= 0)
                triggerVoice(voice, noteOns[evIdx].vel, noteOns[evIdx].vel > 0.75f);
            ++evIdx;
        }

        // Process one sample per voice
        float vs[VOICE_COUNT];
        vs[BD]  = bd_.process();
        vs[SD]  = sd_.process();
        vs[HI]  = hiTom_.process();
        vs[MID] = midTom_.process();
        vs[LO]  = loTom_.process();
        vs[RS]  = rs_.process();
        vs[CP]  = cp_.process();

        // Accumulate main mix (bus 0) and write individual buses
        float mix = 0.0f;
        for (int v = 0; v < VOICE_COUNT; ++v)
        {
            mix += vs[v];
            const int busIdx = VOICE_BUS[v];
            if (busIdx < data.numOutputs && data.outputs[busIdx].numChannels >= 2)
            {
                data.outputs[busIdx].channelBuffers32[0][s] = vs[v];
                data.outputs[busIdx].channelBuffers32[1][s] = vs[v];
            }
        }

        if (data.numOutputs > 0 && data.outputs[0].numChannels >= 2)
        {
            data.outputs[0].channelBuffers32[0][s] = mix * 0.7f;
            data.outputs[0].channelBuffers32[1][s] = mix * 0.7f;
        }
    }

    return kResultTrue;
}

// ---------------------------------------------------------------------------
// State serialisation – binary format: [int8 version][float × kNumParams]
// ---------------------------------------------------------------------------
tresult PLUGIN_API NineProcessor::getState(IBStream* state)
{
    IBStreamer s(state, kLittleEndian);
    s.writeInt8(1); // format version
    for (int i = 0; i < kNumParams; ++i)
        s.writeFloat(params_[i]);
    return kResultTrue;
}

tresult PLUGIN_API NineProcessor::setState(IBStream* state)
{
    IBStreamer s(state, kLittleEndian);
    int8 version = 0;
    if (!s.readInt8(version) || version < 1)
        return kResultFalse;
    for (int i = 0; i < kNumParams; ++i)
    {
        float v = kDefaults[i];
        s.readFloat(v); // tolerate short streams (older presets)
        params_[i] = v;
        applyParam(static_cast<ParamID>(i));
    }
    return kResultTrue;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------
void NineProcessor::applyParam(ParamID pid) noexcept
{
    const float v = params_[pid];
    switch (pid)
    {
        case kBdTune:   bd_.tune    = v; break;
        case kBdDecay:  bd_.decay   = v; break;
        case kBdAttack: bd_.attack  = v; break;
        case kBdLevel:  bd_.level   = v; break;
        case kSdTune:   sd_.tune    = v; break;
        case kSdDecay:  sd_.decay   = v; break;
        case kSdSnappy: sd_.snappy  = v; break;
        case kSdLevel:  sd_.level   = v; break;
        case kHiTune:   hiTom_.tune  = v; break;
        case kHiDecay:  hiTom_.decay = v; break;
        case kHiLevel:  hiTom_.level = v; break;
        case kMidTune:  midTom_.tune  = v; break;
        case kMidDecay: midTom_.decay = v; break;
        case kMidLevel: midTom_.level = v; break;
        case kLoTune:   loTom_.tune  = v; break;
        case kLoDecay:  loTom_.decay = v; break;
        case kLoLevel:  loTom_.level = v; break;
        case kRsLevel:  rs_.level   = v; break;
        case kCpDecay:  cp_.decay   = v; break;
        case kCpLevel:  cp_.level   = v; break;
        default: break;
    }
}

void NineProcessor::applyAllParams() noexcept
{
    for (int i = 0; i < kNumParams; ++i)
        applyParam(static_cast<ParamID>(i));
}

void NineProcessor::triggerVoice(int voice, float velocity, bool accent) noexcept
{
    switch (voice)
    {
        case BD:  bd_.trigger(velocity, accent);     break;
        case SD:  sd_.trigger(velocity, accent);     break;
        case HI:  hiTom_.trigger(velocity, accent);  break;
        case MID: midTom_.trigger(velocity, accent); break;
        case LO:  loTom_.trigger(velocity, accent);  break;
        case RS:  rs_.trigger(velocity, accent);     break;
        case CP:  cp_.trigger(velocity, accent);     break;
        default: break;
    }
}

int NineProcessor::midiNoteToVoice(int note) noexcept
{
    switch (note)
    {
        case 36: return BD;
        case 38: return SD;
        case 45: return HI;
        case 43: return MID;
        case 41: return LO;
        case 37: return RS;
        case 39: return CP;
        default: return -1;
    }
}
