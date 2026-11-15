#pragma once

#include "Widgets.h"

namespace ezui
{
class Page : public juce::Component
{
public:
    virtual void refresh() {}
    void resized() override;

protected:
    std::vector<std::unique_ptr<KnobCell>> cells;
    juce::Rectangle<int> knobArea;      // where the knob row goes (set by subclasses)
    void addCell (APVTS& s, const juce::String& title, const char* knob, const char* power, const juce::String& tip);
    void layoutCells (juce::Rectangle<int> area);
};

//==============================================================================
class MainPage : public Page
{
public:
    explicit MainPage (EZVoiceFXAudioProcessor& p);
    void refresh() override;
};

class CleanPage : public Page
{
public:
    explicit CleanPage (EZVoiceFXAudioProcessor& p);
    void refresh() override;
};

//==============================================================================
class LevelPage : public Page
{
public:
    explicit LevelPage (EZVoiceFXAudioProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;
    void lookAndFeelChanged() override;

private:
    EZVoiceFXAudioProcessor& proc;
    juce::ComboBox mode;
    juce::TextButton learn;
    SwitchToggle autoLearn, assist;
    juce::Label info;
    std::unique_ptr<APVTS::ComboBoxAttachment> modeAtt;
    std::unique_ptr<APVTS::ButtonAttachment> autoAtt, assistAtt;
};

//==============================================================================
class AdvancedPage : public Page
{
public:
    explicit AdvancedPage (EZVoiceFXAudioProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;
    void lookAndFeelChanged() override;

private:
    EZVoiceFXAudioProcessor& proc;
    juce::ComboBox engine, humFreq, listen;
    SwitchToggle mono, matchBypass;
    juce::Label status, info;
    std::unique_ptr<APVTS::ComboBoxAttachment> engineAtt, humAtt, listenAtt;
    std::unique_ptr<APVTS::ButtonAttachment> monoAtt, matchAtt;
};
} // namespace ezui
