#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"
#include "../PluginProcessor.h"
#include "../dsp/FFT.h"

namespace ezui
{
using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
class IconButton : public juce::Button
{
public:
    enum class Icon { Save, Undo, Redo, Gear, Power, Headphones };
    IconButton (Icon i, const juce::String& tip) : juce::Button (tip), icon (i) { setTooltip (tip); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    Icon icon;
};

//==============================================================================
// pill switch with a text label
class SwitchToggle : public juce::ToggleButton
{
public:
    explicit SwitchToggle (const juce::String& text) : juce::ToggleButton (text) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

//==============================================================================
// one knob with its title, value and optional on/off toggle
class KnobCell : public juce::Component
{
public:
    KnobCell (APVTS& s, const juce::String& title, const char* knobId, const char* powerId,
              const juce::String& tooltip, const char* modeId = nullptr,
              const juce::String& modeText = {}, const juce::String& modeTip = {});
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

private:
    juce::String title;
    juce::Slider knob;
    juce::ToggleButton power;
    juce::TextButton mode;
    bool hasPower = false, hasMode = false, lastOn = true;
    juce::String lastText;
    std::unique_ptr<APVTS::SliderAttachment> knobAtt;
    std::unique_ptr<APVTS::ButtonAttachment> powerAtt, modeAtt;
};

//==============================================================================
class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (EZVoiceFXAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void update();   // call from the editor timer

private:
    void analyse (const std::vector<float>& ring, std::vector<float>& outDb);
    float xForFreq (float f) const;
    float yForDb (float db) const;

    EZVoiceFXAudioProcessor& proc;
    static constexpr int kN = 2048, kPoints = 180;
    ez::FFT fft;
    std::vector<float> window, inRing, outRing, tmp, inDb, outDb;
    std::vector<std::complex<float>> buf;
};

//==============================================================================
class HistoryView : public juce::Component
{
public:
    explicit HistoryView (EZVoiceFXAudioProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;

private:
    EZVoiceFXAudioProcessor& proc;
};

//==============================================================================
// Reference vs own vocal tone (10 bands) and the Match EQ curve
class ProfileView : public juce::Component
{
public:
    explicit ProfileView (EZVoiceFXAudioProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;

private:
    EZVoiceFXAudioProcessor& proc;
};

//==============================================================================
class OutputMeter : public juce::Component
{
public:
    explicit OutputMeter (EZVoiceFXAudioProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;

private:
    EZVoiceFXAudioProcessor& proc;
};

//==============================================================================
class SidebarTab : public juce::Button
{
public:
    enum class Icon { Main, Clean, Level, Reference, Advanced };
    SidebarTab (const juce::String& name, Icon i) : juce::Button (name), icon (i) { setClickingTogglesState (false); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    Icon icon;
};

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const Theme& t, float radius = 16.0f);
void drawWaveLogo (juce::Graphics& g, juce::Rectangle<float> r, const Theme& t);
} // namespace ezui
