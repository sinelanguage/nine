#pragma once
/**
 * PluginProcessor – JUCE AudioProcessor for the "nine" TR909 circuit model plugin.
 *
 * MIDI note map (General MIDI + TR909 convention):
 *   36 – Bass Drum
 *   38 – Snare Drum
 *   41 – Low Tom
 *   43 – Mid Tom
 *   45 – Hi Tom
 *   37 – Rim Shot
 *   39 – Hand Clap
 *
 * Bus layout – 7 stereo output buses (one per instrument + one main mix):
 *   Bus 0: Main Mix (always active)
 *   Bus 1: Bass Drum
 *   Bus 2: Snare Drum
 *   Bus 3: Hi Tom
 *   Bus 4: Mid Tom
 *   Bus 5: Lo Tom
 *   Bus 6: Rim Shot
 *   Bus 7: Hand Clap
 */

#pragma once
#include <JuceHeader.h>
#include "circuits/TR909BassDrum.h"
#include "circuits/TR909SnareDrum.h"
#include "circuits/TR909Tom.h"
#include "circuits/TR909RimShot.h"
#include "circuits/TR909Clap.h"

class NineAudioProcessor : public juce::AudioProcessor,
                           public juce::AudioProcessorValueTreeState::Listener
{
public:
    NineAudioProcessor();
    ~NineAudioProcessor() override;

    // AudioProcessor overrides
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override {} // not used

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "nine"; }
    bool   acceptsMidi()  const override { return true; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameter state
    juce::AudioProcessorValueTreeState params;

    // APVTS Listener
    void parameterChanged(const juce::String& paramID, float newValue) override;

private:
    // Circuit model instances
    TR909BassDrum  bd_;
    TR909SnareDrum sd_;
    TR909Tom       hiTom_, midTom_, loTom_;
    TR909RimShot   rs_;
    TR909Clap      cp_;

    // Per-voice pending trigger state (velocity = 0 means no trigger)
    struct Trigger { float velocity = 0.0f; bool accent = false; };
    std::array<Trigger, 7> triggers_ {};  // indexed by VoiceID

    enum VoiceID { BD=0, SD=1, HI=2, MID=3, LO=4, RS=5, CP=6, VOICE_COUNT=7 };

    static int midiNoteToVoice(int note) noexcept;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void applyAllParameters();

    // Bus index for each voice (0 = main mix)
    static constexpr int VOICE_BUS[VOICE_COUNT] = { 1, 2, 3, 4, 5, 6, 7 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NineAudioProcessor)
};
