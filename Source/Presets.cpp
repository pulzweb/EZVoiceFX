#include "Presets.h"

namespace
{
struct Factory
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

const std::vector<Factory>& factory()
{
    static const std::vector<Factory> f = {
        { "Default", {} },
        { "Male Vocal",   { { "lowcut_freq", 90 }, { "denoise_amt", 75 }, { "deess_amt", 45 }, { "deess_freq", 5000 },
                            { "deplosive_amt", 60 }, { "dereverb_amt", 30 }, { "breath_amt", 30 }, { "gate_amt", 40 } } },
        { "Female Vocal", { { "lowcut_freq", 120 }, { "denoise_amt", 75 }, { "deess_amt", 60 }, { "deess_freq", 7000 },
                            { "deplosive_amt", 45 }, { "dereverb_amt", 30 }, { "breath_amt", 35 }, { "gate_amt", 40 } } },
        { "Rap / Hip-Hop", { { "lowcut_freq", 80 }, { "denoise_amt", 85 }, { "deess_amt", 55 }, { "deess_freq", 6000 },
                             { "deplosive_amt", 70 }, { "dereverb_amt", 40 }, { "breath_amt", 45 }, { "gate_amt", 55 },
                             { "level_target", -16 } } },
        { "Whisper / ASMR", { { "lowcut_freq", 70 }, { "denoise_amt", 60 }, { "declick_amt", 65 }, { "deess_amt", 40 },
                              { "deess_freq", 6500 }, { "deplosive_amt", 40 }, { "dereverb_amt", 20 }, { "breath_amt", 10 },
                              { "gate_amt", 20 }, { "level_target", -20 } } },
        { "Podcast / Voice-over", { { "lowcut_freq", 85 }, { "denoise_amt", 85 }, { "deess_amt", 50 }, { "deplosive_amt", 60 },
                                    { "dereverb_amt", 50 }, { "breath_amt", 40 }, { "gate_amt", 60 }, { "level_target", -16 } } },
        { "Backing Vocals", { { "lowcut_freq", 150 }, { "denoise_amt", 80 }, { "deess_amt", 70 }, { "deess_freq", 6500 },
                              { "breath_amt", 60 }, { "gate_amt", 60 }, { "dereverb_amt", 40 }, { "level_target", -20 } } },
        { "Gentle Touch", { { "denoise_amt", 45 }, { "declick_amt", 25 }, { "deess_amt", 30 }, { "deplosive_amt", 30 },
                            { "dereverb_amt", 10 }, { "breath_amt", 15 }, { "gate_amt", 15 } } },
        { "Max Clean", { { "lowcut_freq", 100 }, { "denoise_amt", 100 }, { "declick_amt", 60 }, { "deess_amt", 65 },
                         { "deplosive_amt", 70 }, { "dereverb_amt", 55 }, { "breath_amt", 50 }, { "gate_amt", 70 },
                         { "hum_on", 1 } } },
    };
    return f;
}
} // namespace

juce::StringArray PresetManager::getFactoryNames() const
{
    juce::StringArray s;
    for (auto& f : factory()) s.add (f.name);
    return s;
}

juce::File PresetManager::getUserFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("EZ Voice FX").getChildFile ("Presets");
    if (! dir.exists()) dir.createDirectory();
    return dir;
}

juce::StringArray PresetManager::getUserNames() const
{
    juce::StringArray names;
    for (auto& f : getUserFolder().findChildFiles (juce::File::findFiles, false, "*.ezpreset"))
        names.add (f.getFileNameWithoutExtension());
    names.sort (true);
    return names;
}

void PresetManager::setValue (const juce::String& id, float value)
{
    if (auto* p = state.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void PresetManager::setAllToDefault()
{
    for (auto* p : state.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (rp->getParameterID() == "bypass") continue;
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
}

void PresetManager::loadFactory (int index)
{
    const auto& f = factory();
    if (index < 0 || index >= (int) f.size()) return;
    setAllToDefault();
    for (auto& v : f[(size_t) index].values) setValue (v.first, v.second);
    state.state.setProperty ("presetName", f[(size_t) index].name, nullptr);
}

bool PresetManager::loadUser (const juce::String& name)
{
    const auto file = getUserFolder().getChildFile (name + ".ezpreset");
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr) return false;

    setAllToDefault();
    for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
    {
        const auto id = e->getStringAttribute ("id");
        if (id == "bypass") continue;
        if (auto* p = state.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) e->getDoubleAttribute ("value")));
            p->endChangeGesture();
        }
    }
    state.state.setProperty ("presetName", name, nullptr);
    return true;
}

bool PresetManager::saveUser (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return false;

    juce::XmlElement root ("EZVoiceFXPreset");
    root.setAttribute ("version", "1.1");
    for (auto* p : state.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (rp->getParameterID() == "bypass") continue;
            auto* e = root.createNewChildElement ("PARAM");
            e->setAttribute ("id", rp->getParameterID());
            e->setAttribute ("value", (double) rp->convertFrom0to1 (rp->getValue()));
        }
    const auto file = getUserFolder().getChildFile (name + ".ezpreset");
    if (! root.writeTo (file)) return false;
    state.state.setProperty ("presetName", name, nullptr);
    return true;
}
