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
    void addCell (APVTS& s, const juce::String& title, const char* knob, const char* power, const juce::String& tip,
                  const char* modeId = nullptr, const juce::String& modeText = {}, const juce::String& modeTip = {});
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
    juce::ComboBox mode, group;
    juce::TextButton learn;
    SwitchToggle autoLearn, assist;
    juce::Label info;
    std::unique_ptr<APVTS::ComboBoxAttachment> modeAtt, groupAtt;
    std::unique_ptr<APVTS::ButtonAttachment> autoAtt, assistAtt;
};

//==============================================================================
class RefPage : public Page
{
public:
    explicit RefPage (EZVoiceFXAudioProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;
    void lookAndFeelChanged() override;

private:
    EZVoiceFXAudioProcessor& proc;
    juce::TextButton loadBtn, applyBtn, cancelBtn, clearBtn;
    juce::ComboBox mode;
    SwitchToggle gpu;
    juce::Label status, info;
    std::unique_ptr<APVTS::ComboBoxAttachment> modeAtt;
    std::unique_ptr<APVTS::ButtonAttachment> gpuAtt;
    std::unique_ptr<juce::FileChooser> chooser;
    float progress = 0.0f;
    bool busy = false;
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
    void engineChosen();
    void setEngineParam (int index);

    juce::ComboBox engine, humFreq, listen;
    SwitchToggle mono, matchBypass, whisper;
    juce::Label status, info, hardware;
    std::unique_ptr<APVTS::ComboBoxAttachment> humAtt, listenAtt;
    std::unique_ptr<APVTS::ButtonAttachment> monoAtt, matchAtt, whisperAtt;
    bool confirming = false;
};
} // namespace ezui
