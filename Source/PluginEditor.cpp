#include "PluginEditor.h"

// ---------------------------------------------------------------------------
// Helper: build one instrument section
// ---------------------------------------------------------------------------
void NineAudioProcessorEditor::buildSection(
    VoiceSection& sec,
    const juce::String& title,
    const std::vector<std::pair<juce::String, juce::String>>& paramList)
{
    sec.heading.setText(title, juce::dontSendNotification);
    sec.heading.setFont(juce::Font(14.0f, juce::Font::bold));
    sec.heading.setJustificationType(juce::Justification::centred);
    sec.heading.setColour(juce::Label::textColourId, juce::Colours::orange);
    addAndMakeVisible(sec.heading);

    for (const auto& [paramID, labelText] : paramList) {
        auto* knob = sec.knobs.add(new juce::Slider());
        knob->setSliderStyle(juce::Slider::RotaryVerticalDrag);
        knob->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        knob->setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::orangered);
        knob->setColour(juce::Slider::rotarySliderOutlineColourId,
                        juce::Colours::grey.withAlpha(0.5f));
        addAndMakeVisible(*knob);

        auto* lbl = sec.labels.add(new juce::Label());
        lbl->setText(labelText, juce::dontSendNotification);
        lbl->setFont(juce::Font(11.0f));
        lbl->setJustificationType(juce::Justification::centred);
        lbl->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible(*lbl);

        sec.attachments.add(
            new juce::AudioProcessorValueTreeState::SliderAttachment(
                processor_.params, paramID, *knob));
    }
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
NineAudioProcessorEditor::NineAudioProcessorEditor(NineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor_(p)
{
    using PL = std::vector<std::pair<juce::String, juce::String>>;

    auto addSection = [this](const juce::String& title, PL params) {
        auto* sec = sections_.add(new VoiceSection());
        buildSection(*sec, title, params);
    };

    addSection("BASS DRUM", PL{
        {"bd_tune",  "Tune"},
        {"bd_decay", "Decay"},
        {"bd_tone",  "Tone"},
        {"bd_level", "Level"}
    });
    addSection("SNARE", PL{
        {"sd_tune",   "Tune"},
        {"sd_decay",  "Decay"},
        {"sd_snappy", "Snappy"},
        {"sd_level",  "Level"}
    });
    addSection("HI TOM", PL{
        {"hi_tune",  "Tune"},
        {"hi_decay", "Decay"},
        {"hi_level", "Level"}
    });
    addSection("MID TOM", PL{
        {"mid_tune",  "Tune"},
        {"mid_decay", "Decay"},
        {"mid_level", "Level"}
    });
    addSection("LO TOM", PL{
        {"lo_tune",  "Tune"},
        {"lo_decay", "Decay"},
        {"lo_level", "Level"}
    });
    addSection("RIM SHOT", PL{
        {"rs_level", "Level"}
    });
    addSection("CLAP", PL{
        {"cp_decay", "Decay"},
        {"cp_level", "Level"}
    });

    setSize(820, 200);
    setResizable(false, false);
}

NineAudioProcessorEditor::~NineAudioProcessorEditor() {}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------
void NineAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1a1a2e));

    // Section dividers
    g.setColour(juce::Colours::grey.withAlpha(0.3f));
    int x = 0;
    for (int i = 0; i < sections_.size(); ++i) {
        int numKnobs = sections_[i]->knobs.size();
        int w = SECTION_PAD * 2 + numKnobs * KNOB_SIZE;
        if (i > 0)
            g.drawVerticalLine(x, 4, getHeight() - 4);
        x += w;
    }

    g.setColour(juce::Colours::orangered);
    g.setFont(juce::Font("Courier", 16.0f, juce::Font::bold));
    g.drawText("nine  |  TR-909 circuit model", 0, getHeight() - 22,
               getWidth(), 20, juce::Justification::centred);
}

// ---------------------------------------------------------------------------
// Resized
// ---------------------------------------------------------------------------
void NineAudioProcessorEditor::resized() {
    const int knobTop  = SECTION_PAD + HEADING_H;
    const int labelTop = knobTop + KNOB_SIZE;

    int xOffset = 0;
    for (auto* sec : sections_) {
        const int numKnobs = sec->knobs.size();
        const int secW     = SECTION_PAD * 2 + numKnobs * KNOB_SIZE;

        sec->heading.setBounds(xOffset, SECTION_PAD, secW, HEADING_H);

        for (int i = 0; i < numKnobs; ++i) {
            const int kx = xOffset + SECTION_PAD + i * KNOB_SIZE;
            sec->knobs[i]->setBounds(kx, knobTop, KNOB_SIZE, KNOB_SIZE);
            sec->labels[i]->setBounds(kx, labelTop, KNOB_SIZE, LABEL_H);
        }

        xOffset += secW;
    }
}

