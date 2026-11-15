#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "EZLookAndFeel.h"

//==============================================================================
class ModuleCard : public juce::Component
{
public:
    ModuleCard (juce::AudioProcessorValueTreeState& state,
                const juce::String& title, const juce::String& subtitle,
                const char* knobParamId, const char* powerParamId,
                juce::Colour accent, const juce::String& tooltip, bool aiBadge = false);

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();                 // called from the editor timer

    void setMeterValue (float v01) { meter = juce::jlimit (0.0f, 1.0f, v01); }

protected:
    juce::Rectangle<int> knobArea;  // subclasses may change the layout
    juce::Slider knob;
    juce::ToggleButton power;
    juce::Colour accent;

private:
    juce::String title, subtitle;
    bool hasPower = false, ai = false, lastOn = true;
    float meter = 0.0f, lastMeter = -1.0f;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
};

//==============================================================================
class AutoLevelCard : public ModuleCard
{
public:
    AutoLevelCard (EZVoiceFXAudioProcessor& p);
    void resized() override;
    void refreshLearn();

private:
    EZVoiceFXAudioProcessor& proc;
    juce::ComboBox mode;
    juce::TextButton learn;
    juce::Label info;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt;
};

//==============================================================================
class MeterPanel : public juce::Component
{
public:
    explicit MeterPanel (EZVoiceFXAudioProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;

private:
    void drawBar (juce::Graphics&, juce::Rectangle<float> r, const juce::String& label,
                  float valueDb, float minDb, float maxDb, juce::Colour c, const juce::String& text);
    EZVoiceFXAudioProcessor& proc;
};

//==============================================================================
class EZVoiceFXAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EZVoiceFXAudioProcessorEditor (EZVoiceFXAudioProcessor&);
    ~EZVoiceFXAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    EZVoiceFXAudioProcessor& proc;
    ezui::EZLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };

    std::unique_ptr<ModuleCard> lowcut, declick, denoise, deplosive, dereverb, breath, deess, limiter;
    std::unique_ptr<AutoLevelCard> level;
    MeterPanel meterPanel;

    juce::Slider outKnob;
    juce::TextButton bypassBtn { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;

    int lastClicks = 0;
    float clickFlash = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EZVoiceFXAudioProcessorEditor)
};
