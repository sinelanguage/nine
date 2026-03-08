#pragma once
/**
 * Plugin editor (UI) for the nine TR909 plugin.
 *
 * Shows a row of knobs for each instrument:
 *   BD: Tune | Decay | Tone | Level
 *   SD: Tune | Decay | Snappy | Level
 *   Hi/Mid/Lo Tom: Tune | Decay | Level
 *   RS: Level
 *   CP: Decay | Level
 */

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class NineAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit NineAudioProcessorEditor(NineAudioProcessor& p);
    ~NineAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    NineAudioProcessor& processor_;

    struct VoiceSection {
        juce::Label heading;
        juce::OwnedArray<juce::Slider> knobs;
        juce::OwnedArray<juce::Label>  labels;
        juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> attachments;
    };

    // Store sections as unique_ptr so VoiceSection (with OwnedArray) is heap-allocated
    juce::OwnedArray<VoiceSection> sections_;

    void buildSection(VoiceSection& sec,
                      const juce::String& title,
                      const std::vector<std::pair<juce::String, juce::String>>& params);

    static constexpr int KNOB_SIZE   = 60;
    static constexpr int LABEL_H     = 16;
    static constexpr int SECTION_PAD = 8;
    static constexpr int HEADING_H   = 20;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NineAudioProcessorEditor)
};

