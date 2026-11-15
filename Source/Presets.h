#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Factory presets (built in) + user presets (Documents/EZ Voice FX/Presets/*.ezpreset)
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& s) : state (s) {}

    static constexpr int kUserIdBase = 100;

    juce::StringArray getFactoryNames() const;
    juce::StringArray getUserNames() const;

    void loadFactory (int index);
    bool loadUser (const juce::String& name);
    bool saveUser (const juce::String& name);
    void resetToDefault() { loadFactory (0); }

    juce::String getCurrentName() const { return state.state.getProperty ("presetName", "Default").toString(); }

    static juce::File getUserFolder();

private:
    void setValue (const juce::String& id, float value);
    void setAllToDefault();

    juce::AudioProcessorValueTreeState& state;
};
