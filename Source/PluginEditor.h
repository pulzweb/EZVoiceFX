#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "Presets.h"
#include "ui/LookAndFeel.h"
#include "ui/Widgets.h"
#include "ui/Pages.h"
#include "ui/Magic.h"

//==============================================================================
// The whole interface at its base size (1100 x 700). The editor scales it.
class MainUI : public juce::Component
{
public:
    static constexpr int kW = 1100, kH = 700;

    MainUI (EZVoiceFXAudioProcessor& p, ezui::EZLookAndFeel& lnf);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();                 // 30 Hz from the editor timer

    std::function<void (int)> onThemeChange;
    std::function<void (float)> onScaleChange;
    float currentScale = 1.0f;

private:
    void selectPage (int index);
    void refreshPresetList();
    void savePreset();
    void showSettingsMenu();
    void startMagicFlow();
    void setRole (bool beat);
    void updateRoleVisibility();

    EZVoiceFXAudioProcessor& proc;
    ezui::EZLookAndFeel& lnf;
    PresetManager presets;

    juce::ComboBox presetBox;
    ezui::IconButton saveBtn { ezui::IconButton::Icon::Save, juce::String::fromUTF8 ("Сохранить пресет") };
    ezui::IconButton undoBtn { ezui::IconButton::Icon::Undo, juce::String::fromUTF8 ("Отменить") };
    ezui::IconButton redoBtn { ezui::IconButton::Icon::Redo, juce::String::fromUTF8 ("Повторить") };
    ezui::IconButton gearBtn { ezui::IconButton::Icon::Gear, juce::String::fromUTF8 ("Настройки: тема, размер окна") };

    ezui::SidebarTab tabMain { "Main", ezui::SidebarTab::Icon::Main };
    ezui::SidebarTab tabClean { "Clean", ezui::SidebarTab::Icon::Clean };
    ezui::SidebarTab tabLevel { "Level", ezui::SidebarTab::Icon::Level };
    ezui::SidebarTab tabRef { "Reference", ezui::SidebarTab::Icon::Reference };
    ezui::SidebarTab tabAdv { "Advanced", ezui::SidebarTab::Icon::Advanced };

    ezui::SpectrumView spectrum;
    ezui::HistoryView history;
    ezui::OutputMeter meter;
    ezui::ProfileView profileView;

    static constexpr int kPages = 5;
    std::unique_ptr<ezui::Page> pages[kPages];
    int currentPage = 0;

    juce::Slider mixSlider, outSlider;
    ezui::IconButton listenBtn { ezui::IconButton::Icon::Headphones, juce::String::fromUTF8 ("Слушать удалённое (что убирает плагин)") };
    ezui::IconButton powerBtn { ezui::IconButton::Icon::Power, juce::String::fromUTF8 ("Вкл / Bypass") };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAtt, outAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> listenAtt, powerAtt;

    juce::TextButton magicBtn;
    std::unique_ptr<ezui::WelcomeOverlay> welcome;
    ezui::MagicOverlay magic;
    ezui::BeatPanel beatPanel;

    juce::String lastPresetName, lastBottomText;
    bool lastPro = false, lastBeatRole = false, magicVisible = false;
    int lastBeats = -1;
};

//==============================================================================
class EZVoiceFXAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EZVoiceFXAudioProcessorEditor (EZVoiceFXAudioProcessor&);
    ~EZVoiceFXAudioProcessorEditor() override;

    void resized() override;

private:
    void timerCallback() override;
    void applyTheme (int index);
    void applyScale (float s);

    EZVoiceFXAudioProcessor& proc;
    ezui::EZLookAndFeel lnf;
    std::unique_ptr<juce::PropertiesFile> settings;
    juce::TooltipWindow tooltips { this, 700 };
    MainUI ui;
    int tick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EZVoiceFXAudioProcessorEditor)
};
