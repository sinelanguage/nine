#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>

// ---------------------------------------------------------------------------
// MIDI note → voice index
// ---------------------------------------------------------------------------
int NineAudioProcessor::midiNoteToVoice(int note) noexcept {
    switch (note) {
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

// ---------------------------------------------------------------------------
// Parameter layout
// ---------------------------------------------------------------------------
juce::AudioProcessorValueTreeState::ParameterLayout
NineAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto add = [&](const juce::String& id, const juce::String& name,
                   float min, float max, float def) {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ id, 1 }, name,
            juce::NormalisableRange<float>(min, max), def));
    };

    // Bass Drum
    add("bd_tune",  "BD Tune",  0.0f, 1.0f, 0.5f);
    add("bd_decay", "BD Decay", 0.0f, 1.0f, 0.5f);
    add("bd_tone",  "BD Tone",  0.0f, 1.0f, 0.5f);
    add("bd_level", "BD Level", 0.0f, 1.0f, 0.8f);

    // Snare
    add("sd_tune",   "SD Tune",   0.0f, 1.0f, 0.5f);
    add("sd_decay",  "SD Decay",  0.0f, 1.0f, 0.5f);
    add("sd_snappy", "SD Snappy", 0.0f, 1.0f, 0.5f);
    add("sd_level",  "SD Level",  0.0f, 1.0f, 0.8f);

    // Hi Tom
    add("hi_tune",  "Hi Tom Tune",  0.0f, 1.0f, 0.6f);
    add("hi_decay", "Hi Tom Decay", 0.0f, 1.0f, 0.4f);
    add("hi_level", "Hi Tom Level", 0.0f, 1.0f, 0.8f);

    // Mid Tom
    add("mid_tune",  "Mid Tom Tune",  0.0f, 1.0f, 0.5f);
    add("mid_decay", "Mid Tom Decay", 0.0f, 1.0f, 0.5f);
    add("mid_level", "Mid Tom Level", 0.0f, 1.0f, 0.8f);

    // Lo Tom
    add("lo_tune",  "Lo Tom Tune",  0.0f, 1.0f, 0.4f);
    add("lo_decay", "Lo Tom Decay", 0.0f, 1.0f, 0.6f);
    add("lo_level", "Lo Tom Level", 0.0f, 1.0f, 0.8f);

    // Rim Shot
    add("rs_level", "RS Level", 0.0f, 1.0f, 0.8f);

    // Clap
    add("cp_decay", "CP Decay", 0.0f, 1.0f, 0.5f);
    add("cp_level", "CP Level", 0.0f, 1.0f, 0.8f);

    return { params.begin(), params.end() };
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
NineAudioProcessor::NineAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withOutput("Main Mix", juce::AudioChannelSet::stereo(), true)
          .withOutput("Bass Drum", juce::AudioChannelSet::stereo(), true)
          .withOutput("Snare",     juce::AudioChannelSet::stereo(), true)
          .withOutput("Hi Tom",    juce::AudioChannelSet::stereo(), true)
          .withOutput("Mid Tom",   juce::AudioChannelSet::stereo(), true)
          .withOutput("Lo Tom",    juce::AudioChannelSet::stereo(), true)
          .withOutput("Rim Shot",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Clap",      juce::AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "nine_params", createLayout())
{
    hiTom_.setType(TomType::Hi);
    midTom_.setType(TomType::Mid);
    loTom_.setType(TomType::Lo);

    // Register as listener for all parameters
    for (auto* p : params.processor.getParameters())
        if (auto* fp = dynamic_cast<juce::AudioParameterFloat*>(p))
            params.addParameterListener(fp->getParameterID(), this);
}

NineAudioProcessor::~NineAudioProcessor() {}

// ---------------------------------------------------------------------------
// Prepare
// ---------------------------------------------------------------------------
void NineAudioProcessor::prepareToPlay(double sampleRate, int /*blockSize*/) {
    bd_.prepare(sampleRate);
    sd_.prepare(sampleRate);
    hiTom_.prepare(sampleRate);
    midTom_.prepare(sampleRate);
    loTom_.prepare(sampleRate);
    rs_.prepare(sampleRate);
    cp_.prepare(sampleRate);

    applyAllParameters();
}

void NineAudioProcessor::releaseResources() {}

// ---------------------------------------------------------------------------
// Bus layout support
// ---------------------------------------------------------------------------
bool NineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    // We require all outputs to be stereo (or disabled)
    for (int b = 0; b < layouts.outputBuses.size(); ++b) {
        const auto& ch = layouts.outputBuses[b];
        if (!ch.isDisabled() && ch != juce::AudioChannelSet::stereo())
            return false;
    }
    // Must have at least main mix output
    if (layouts.getMainOutputChannelSet().isDisabled())
        return false;
    return true;
}

// ---------------------------------------------------------------------------
// processBlock
// ---------------------------------------------------------------------------
void NineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int totalSamples = buffer.getNumSamples();
    int samplePos = 0;

    // Iterate MIDI events and audio together
    for (const auto meta : midiMessages) {
        const auto msg = meta.getMessage();
        const int  msgSample = meta.samplePosition;

        // Render up to this MIDI event
        for (; samplePos < msgSample && samplePos < totalSamples; ++samplePos) {
            float voices[VOICE_COUNT];
            voices[BD]  = bd_.process();
            voices[SD]  = sd_.process();
            voices[HI]  = hiTom_.process();
            voices[MID] = midTom_.process();
            voices[LO]  = loTom_.process();
            voices[RS]  = rs_.process();
            voices[CP]  = cp_.process();

            float mix = 0.0f;
            for (int v = 0; v < VOICE_COUNT; ++v) {
                mix += voices[v];
                // Write to individual bus
                const int bus = VOICE_BUS[v];
                if (bus < buffer.getNumChannels() / 2) {
                    int ch = bus * 2;
                    if (ch + 1 < buffer.getNumChannels()) {
                        buffer.addSample(ch,     samplePos, voices[v]);
                        buffer.addSample(ch + 1, samplePos, voices[v]);
                    }
                }
            }
            // Main mix (bus 0 = channels 0,1)
            if (buffer.getNumChannels() >= 2) {
                buffer.addSample(0, samplePos, mix * 0.7f);
                buffer.addSample(1, samplePos, mix * 0.7f);
            }
        }

        // Process MIDI event
        if (msg.isNoteOn()) {
            const int voice = midiNoteToVoice(msg.getNoteNumber());
            if (voice >= 0) {
                const float vel    = msg.getFloatVelocity();
                const bool  accent = (vel > 0.75f);
                switch (voice) {
                    case BD:  bd_.trigger(vel, accent);     break;
                    case SD:  sd_.trigger(vel, accent);     break;
                    case HI:  hiTom_.trigger(vel, accent);  break;
                    case MID: midTom_.trigger(vel, accent); break;
                    case LO:  loTom_.trigger(vel, accent);  break;
                    case RS:  rs_.trigger(vel, accent);     break;
                    case CP:  cp_.trigger(vel, accent);     break;
                    default: break;
                }
            }
        }
    }

    // Render remaining samples after last MIDI event
    for (; samplePos < totalSamples; ++samplePos) {
        float voices[VOICE_COUNT];
        voices[BD]  = bd_.process();
        voices[SD]  = sd_.process();
        voices[HI]  = hiTom_.process();
        voices[MID] = midTom_.process();
        voices[LO]  = loTom_.process();
        voices[RS]  = rs_.process();
        voices[CP]  = cp_.process();

        float mix = 0.0f;
        for (int v = 0; v < VOICE_COUNT; ++v) {
            mix += voices[v];
            const int bus = VOICE_BUS[v];
            int ch = bus * 2;
            if (ch + 1 < buffer.getNumChannels()) {
                buffer.addSample(ch,     samplePos, voices[v]);
                buffer.addSample(ch + 1, samplePos, voices[v]);
            }
        }
        if (buffer.getNumChannels() >= 2) {
            buffer.addSample(0, samplePos, mix * 0.7f);
            buffer.addSample(1, samplePos, mix * 0.7f);
        }
    }
}

// ---------------------------------------------------------------------------
// Parameter callbacks
// ---------------------------------------------------------------------------
void NineAudioProcessor::parameterChanged(const juce::String& paramID, float newValue) {
    if      (paramID == "bd_tune")   bd_.tune   = newValue;
    else if (paramID == "bd_decay")  bd_.decay  = newValue;
    else if (paramID == "bd_tone")   bd_.tone   = newValue;
    else if (paramID == "bd_level")  bd_.level  = newValue;
    else if (paramID == "sd_tune")   sd_.tune   = newValue;
    else if (paramID == "sd_decay")  sd_.decay  = newValue;
    else if (paramID == "sd_snappy") sd_.snappy = newValue;
    else if (paramID == "sd_level")  sd_.level  = newValue;
    else if (paramID == "hi_tune")   hiTom_.tune  = newValue;
    else if (paramID == "hi_decay")  hiTom_.decay = newValue;
    else if (paramID == "hi_level")  hiTom_.level = newValue;
    else if (paramID == "mid_tune")  midTom_.tune  = newValue;
    else if (paramID == "mid_decay") midTom_.decay = newValue;
    else if (paramID == "mid_level") midTom_.level = newValue;
    else if (paramID == "lo_tune")   loTom_.tune  = newValue;
    else if (paramID == "lo_decay")  loTom_.decay = newValue;
    else if (paramID == "lo_level")  loTom_.level = newValue;
    else if (paramID == "rs_level")  rs_.level  = newValue;
    else if (paramID == "cp_decay")  cp_.decay  = newValue;
    else if (paramID == "cp_level")  cp_.level  = newValue;
}

void NineAudioProcessor::applyAllParameters() {
    for (auto* p : params.processor.getParameters()) {
        if (auto* fp = dynamic_cast<juce::AudioParameterFloat*>(p))
            parameterChanged(fp->getParameterID(), fp->get());
    }
}

// ---------------------------------------------------------------------------
// State save / load
// ---------------------------------------------------------------------------
void NineAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = params.copyState();
    auto xml   = state.createXml();
    copyXmlToBinary(*xml, destData);
}

void NineAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml && xml->hasTagName(params.state.getType()))
        params.replaceState(juce::ValueTree::fromXml(*xml));
}

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------
juce::AudioProcessorEditor* NineAudioProcessor::createEditor() {
    return new NineAudioProcessorEditor(*this);
}

// ---------------------------------------------------------------------------
// Plugin entry point
// ---------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new NineAudioProcessor();
}
