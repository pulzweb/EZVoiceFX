#pragma once

#include "Widgets.h"

namespace ezui
{
//==============================================================================
// First screen of a new instance: Magic / Manual / Beat Link
class WelcomeOverlay : public juce::Component
{
public:
    WelcomeOverlay();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void()> onMagic, onManual, onBeat;

private:
    juce::Rectangle<float> card (int i) const;
    int hover = -1;
};

//==============================================================================
// "Magic" analysis screen with live visualisation
class MagicOverlay : public juce::Component
{
public:
    explicit MagicOverlay (EZVoiceFXAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();                     // 30 Hz

    std::function<void()> onCancel, onFinish, onManual, onAgain;

private:
    EZVoiceFXAudioProcessor& proc;
    juce::TextButton cancelBtn, finishBtn, manualBtn, againBtn;
    float phase = 0.0f, level = 0.0f, analyzeSpin = 0.0f;
    bool voiceSeen = false, beatSeen = false;
    float vocalBars[ez::LinkHub::kBands] {}, beatBars[ez::LinkHub::kBands] {};
    int lastState = -1;
};

//==============================================================================
// UI of an instance in "Beat Link" role
class BeatPanel : public juce::Component
{
public:
    explicit BeatPanel (EZVoiceFXAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

    std::function<void()> onMakeVocal;

private:
    EZVoiceFXAudioProcessor& proc;
    KnobCell unmaskKnob;
    juce::TextButton vocalBtn;
    float own[ez::LinkHub::kBands] {}, other[ez::LinkHub::kBands] {};
};
} // namespace ezui
