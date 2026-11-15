#include "PluginEditor.h"

namespace C = ezui::Colours;
using ezui::font;

static juce::String ru (const char* s) { return juce::String::fromUTF8 (s); }

//==============================================================================
// ModuleCard
//==============================================================================
ModuleCard::ModuleCard (juce::AudioProcessorValueTreeState& state,
                        const juce::String& t, const juce::String& sub,
                        const char* knobParamId, const char* powerParamId,
                        juce::Colour a, const juce::String& tooltip, bool aiBadge)
    : accent (a), title (t), subtitle (sub), ai (aiBadge)
{
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 100, 20);
    knob.setColour (juce::Slider::rotarySliderFillColourId, accent);
    knob.setTooltip (tooltip);
    addAndMakeVisible (knob);
    knobAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, knobParamId, knob);

    if (auto* rp = state.getParameter (knobParamId))
        knob.setDoubleClickReturnValue (true, rp->convertFrom0to1 (rp->getDefaultValue()));

    if (powerParamId != nullptr)
    {
        hasPower = true;
        power.setColour (juce::ToggleButton::tickColourId, accent);
        power.setTooltip (ru ("Включить / выключить модуль"));
        addAndMakeVisible (power);
        powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, powerParamId, power);
    }
}

void ModuleCard::resized()
{
    const auto b = getLocalBounds();
    if (hasPower) power.setBounds (b.getWidth() - 44, 12, 30, 30);
    knobArea = juce::Rectangle<int> (10, 50, b.getWidth() - 20, b.getHeight() - 50 - 26);
    knob.setBounds (knobArea);
}

void ModuleCard::refresh()
{
    const bool on = ! hasPower || power.getToggleState();
    if (on != lastOn)
    {
        lastOn = on;
        knob.setAlpha (on ? 1.0f : 0.38f);
        repaint();
    }
    if (std::abs (meter - lastMeter) > 0.01f)
    {
        lastMeter = meter;
        repaint (0, getHeight() - 24, getWidth(), 24);
    }
}

void ModuleCard::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = ! hasPower || power.getToggleState();

    juce::ColourGradient bg (C::card, r.getX(), r.getY(), C::cardDark, r.getX(), r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, 14.0f);

    // accent strip at the top
    {
        auto strip = r.withHeight (3.0f).reduced (18.0f, 0.0f).translated (0.0f, 1.0f);
        juce::ColourGradient sg (accent.withAlpha (on ? 0.95f : 0.25f), strip.getX(), strip.getY(),
                                 accent.withAlpha (0.0f), strip.getRight(), strip.getY(), false);
        g.setGradientFill (sg);
        g.fillRoundedRectangle (strip, 1.5f);
    }

    g.setColour (accent.withAlpha (on ? 0.40f : 0.10f));
    g.drawRoundedRectangle (r, 14.0f, 1.2f);

    g.setColour (on ? C::text : C::textDim);
    g.setFont (font (15.0f, true));
    g.drawText (title, 16, 12, getWidth() - 70, 20, juce::Justification::centredLeft);

    g.setColour (C::textDim);
    g.setFont (font (11.5f));
    g.drawText (subtitle, 16, 31, getWidth() - 70, 16, juce::Justification::centredLeft);

    if (ai)
    {
        const juce::Rectangle<float> pill ((float) getWidth() - 86.0f, 16.0f, 34.0f, 18.0f);
        juce::ColourGradient pg (C::violet, pill.getX(), pill.getY(), C::cyan, pill.getRight(), pill.getBottom(), false);
        g.setGradientFill (pg);
        g.fillRoundedRectangle (pill, 9.0f);
        g.setColour (juce::Colours::white);
        g.setFont (font (11.0f, true));
        g.drawText ("AI", pill, juce::Justification::centred);
    }

    // activity meter
    const juce::Rectangle<float> bar (18.0f, (float) getHeight() - 16.0f, (float) getWidth() - 36.0f, 4.0f);
    g.setColour (C::track);
    g.fillRoundedRectangle (bar, 2.0f);
    if (on && meter > 0.001f)
    {
        g.setColour (accent.withAlpha (0.9f));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * meter), 2.0f);
    }
}

//==============================================================================
// AutoLevelCard
//==============================================================================
AutoLevelCard::AutoLevelCard (EZVoiceFXAudioProcessor& p)
    : ModuleCard (p.apvts, "AUTO-LEVEL", ru ("Одинаковая громкость каждой капы"),
                  ParamIDs::levelTarget, ParamIDs::levelOn, C::level,
                  ru ("Целевая громкость вокала (LUFS). -18 LUFS оставляет запас для автотюна, реверба и компрессии.")),
      proc (p)
{
    mode.addItemList ({ "Ride (auto)", "Learn (exact)" }, 1);
    mode.setTooltip (ru ("Ride: плагин сам ведёт громкость в реальном времени.\n"
                         "Learn: проиграйте капу один раз, плагин измерит её и выставит точное усиление."));
    addAndMakeVisible (mode);
    modeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ParamIDs::levelMode, mode);

    learn.setColour (juce::TextButton::buttonOnColourId, C::level.darker (0.2f));
    learn.setButtonText (ru ("АНАЛИЗ (LEARN)"));
    learn.setTooltip (ru ("Нажмите, проиграйте капу целиком, затем нажмите ещё раз."));
    learn.onClick = [this]
    {
        if (proc.isLearning())
        {
            proc.stopLearn();
        }
        else
        {
            proc.startLearn();
            if (auto* prm = proc.apvts.getParameter (ParamIDs::levelMode))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (1.0f);
                prm->endChangeGesture();
            }
        }
    };
    addAndMakeVisible (learn);

    info.setFont (font (12.0f));
    info.setColour (juce::Label::textColourId, C::textDim);
    info.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (info);
}

void AutoLevelCard::resized()
{
    ModuleCard::resized();
    const int w = getWidth(), h = getHeight();
    knob.setBounds (10, 50, w / 2 - 10, h - 76);
    auto right = juce::Rectangle<int> (w / 2 + 6, 58, w / 2 - 24, h - 84);
    mode.setBounds (right.removeFromTop (30));
    right.removeFromTop (10);
    learn.setBounds (right.removeFromTop (40));
    right.removeFromTop (8);
    info.setBounds (right);
}

void AutoLevelCard::refreshLearn()
{
    const bool learning = proc.isLearning();
    learn.setToggleState (learning, juce::dontSendNotification);
    learn.setButtonText (learning ? ru ("СТОП И ПРИМЕНИТЬ") : ru ("АНАЛИЗ (LEARN)"));

    const bool learnMode = proc.apvts.getRawParameterValue (ParamIDs::levelMode)->load() >= 0.5f;
    const float target = proc.apvts.getRawParameterValue (ParamIDs::levelTarget)->load();
    const float gain = proc.meters.autoGainDb.load();
    const auto gainTxt = (gain >= 0.0f ? "+" : "") + juce::String (gain, 1) + " dB";

    juce::String txt;
    if (learning)
        txt = ru ("Идёт замер...\nПроиграйте всю капу и нажмите СТОП.");
    else if (learnMode && proc.hasLearned())
    {
        const float lufs = proc.getLearnedLufs();
        const float g = juce::jlimit (-30.0f, 30.0f, target - lufs);
        txt = ru ("Замер: ") + juce::String (lufs, 1) + " LUFS\n" + ru ("Усиление: ")
              + (g >= 0.0f ? "+" : "") + juce::String (g, 1) + " dB";
    }
    else if (learnMode)
        txt = ru ("Нажмите АНАЛИЗ и проиграйте капу целиком.\nСейчас работает Ride.");
    else
        txt = ru ("Громкость выравнивается автоматически.\nУсиление: ") + gainTxt;

    if (info.getText() != txt) info.setText (txt, juce::dontSendNotification);
}

//==============================================================================
// MeterPanel
//==============================================================================
void MeterPanel::drawBar (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& label,
                          float valueDb, float minDb, float maxDb, juce::Colour c, const juce::String& text)
{
    g.setColour (C::textDim);
    g.setFont (font (11.0f, true));
    g.drawText (label, r.removeFromLeft (36.0f), juce::Justification::centredLeft);
    auto txtArea = r.removeFromRight (70.0f);
    const auto bar = r.withSizeKeepingCentre (r.getWidth(), 8.0f);
    g.setColour (C::track);
    g.fillRoundedRectangle (bar, 4.0f);
    const float p = juce::jlimit (0.0f, 1.0f, (valueDb - minDb) / (maxDb - minDb));
    if (p > 0.0f)
    {
        juce::ColourGradient cg (c.darker (0.3f), bar.getX(), 0.0f, c.brighter (0.3f), bar.getRight(), 0.0f, false);
        g.setGradientFill (cg);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * p), 4.0f);
    }
    g.setColour (C::text);
    g.setFont (font (12.0f, true));
    g.drawText (text, txtArea, juce::Justification::centredRight);
}

void MeterPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (C::cardDark.withAlpha (0.9f));
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (C::track);
    g.drawRoundedRectangle (r, 14.0f, 1.0f);

    auto& m = proc.meters;
    const float inDb = m.inPeakDb.load(), outDb = m.outPeakDb.load();
    const auto dbTxt = [] (float v) { return v <= -99.0f ? juce::String ("-inf") : juce::String (v, 1) + " dB"; };

    auto area = r.reduced (16.0f, 12.0f);
    auto bars = area.removeFromLeft (300.0f);
    drawBar (g, bars.removeFromTop (bars.getHeight() * 0.5f), "IN", inDb, -60.0f, 0.0f, C::lowcut, dbTxt (inDb));
    drawBar (g, bars, "OUT", outDb, -60.0f, 0.0f, C::level, dbTxt (outDb));

    area.removeFromLeft (24.0f);

    auto block = [&g] (juce::Rectangle<float> b, const juce::String& label, const juce::String& value,
                       juce::Colour c, const juce::String& unit)
    {
        g.setColour (C::textDim);
        g.setFont (font (10.5f, true));
        g.drawText (label, b.removeFromTop (16.0f), juce::Justification::centredLeft);
        g.setColour (c);
        g.setFont (font (24.0f, true));
        auto v = b.removeFromTop (30.0f);
        g.drawText (value, v, juce::Justification::centredLeft);
        g.setColour (C::textDim);
        g.setFont (font (10.5f));
        g.drawText (unit, b, juce::Justification::centredLeft);
    };

    const float lufs = m.outLufs.load();
    block (area.removeFromLeft (150.0f), "OUTPUT LOUDNESS",
           lufs < -60.0f ? juce::String ("--") : juce::String (lufs, 1), C::level, "LUFS");

    const float gdb = m.autoGainDb.load();
    block (area.removeFromLeft (120.0f), "AUTO GAIN",
           (gdb >= 0.0f ? "+" : "") + juce::String (gdb, 1), C::text, "dB");

    block (area.removeFromLeft (110.0f), "AI VOICE",
           juce::String (juce::roundToInt (m.vad.load() * 100.0f)), C::denoise, "% speech");

    const double latMs = 1000.0 * m.latencySamples.load() / juce::jmax (1.0, m.sampleRate.load());
    block (area, "LATENCY", juce::String (latMs, 1), C::textDim.brighter (0.3f),
           "ms @ " + juce::String (m.sampleRate.load() / 1000.0, 1) + " kHz");
}

//==============================================================================
// Editor
//==============================================================================
EZVoiceFXAudioProcessorEditor::EZVoiceFXAudioProcessorEditor (EZVoiceFXAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), meterPanel (p)
{
    setLookAndFeel (&lnf);
    using namespace ParamIDs;
    auto& s = p.apvts;

    lowcut = std::make_unique<ModuleCard> (s, "LOW CUT", ru ("Гул, вибрации стойки"), lowcutFreq, lowcutOn, C::lowcut,
        ru ("Срезает низкий гул и вибрации ниже выбранной частоты. 80 Гц подходит почти всем голосам."));
    declick = std::make_unique<ModuleCard> (s, "DE-CLICK", ru ("Щелчки и чмоки рта"), declickAmt, declickOn, C::declick,
        ru ("Находит короткие щелчки (чмоки, слюна, цифровые тики) и аккуратно восстанавливает звук на их месте."));
    denoise = std::make_unique<ModuleCard> (s, "DENOISE", ru ("Нейросеть RNNoise"), denoiseAmt, denoiseOn, C::denoise,
        ru ("Локальная нейросеть RNNoise убирает фоновый шум: компьютер, кондиционер, улица, шипение. Работает без интернета."), true);
    deplosive = std::make_unique<ModuleCard> (s, "DE-PLOSIVE", ru ("Удары воздуха П и Б"), deplosiveAmt, deplosiveOn, C::deplosive,
        ru ("Гасит низкочастотные удары воздуха в микрофон на звуках П, Б."));
    dereverb = std::make_unique<ModuleCard> (s, "DE-REVERB", ru ("Отзвук комнаты"), dereverbAmt, dereverbOn, C::dereverb,
        ru ("Подавляет хвосты комнатного отзвука. Больше 60% может звучать неестественно."));
    breath = std::make_unique<ModuleCard> (s, "BREATH", ru ("Тише вдохи"), breathAmt, breathOn, C::breath,
        ru ("Делает вдохи между фразами тише (до -20 дБ), не вырезая их полностью."));
    deess = std::make_unique<ModuleCard> (s, "DE-ESS", ru ("Резкие С, Ш, Щ"), deessAmt, deessOn, C::deess,
        ru ("Приглушает резкие свистящие и шипящие звуки, только в момент их появления."));
    limiter = std::make_unique<ModuleCard> (s, "LIMITER", ru ("Потолок пиков"), limiterCeil, limiterOn, C::limiter,
        ru ("Не даёт пикам превысить потолок. Рекомендуется -1 дБ."));
    level = std::make_unique<AutoLevelCard> (p);

    for (auto* c : { lowcut.get(), declick.get(), denoise.get(), deplosive.get(), dereverb.get(),
                     breath.get(), deess.get(), limiter.get() })
        addAndMakeVisible (*c);
    addAndMakeVisible (*level);
    addAndMakeVisible (meterPanel);

    outKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    outKnob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    outKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    outKnob.setColour (juce::Slider::rotarySliderFillColourId, C::output);
    outKnob.setTooltip (ru ("Общая громкость на выходе (перед лимитером)."));
    addAndMakeVisible (outKnob);
    outAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, outputGain, outKnob);
    outKnob.setDoubleClickReturnValue (true, 0.0);

    bypassBtn.setClickingTogglesState (true);
    bypassBtn.setColour (juce::TextButton::buttonOnColourId, C::limiter);
    bypassBtn.setTooltip (ru ("Сравнить с исходной капой (с учётом задержки)."));
    addAndMakeVisible (bypassBtn);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, bypass, bypassBtn);

    setSize (1060, 700);
    startTimerHz (30);
}

EZVoiceFXAudioProcessorEditor::~EZVoiceFXAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void EZVoiceFXAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    juce::ColourGradient bg (C::bgTop, 0.0f, 0.0f, C::bgBottom, 0.0f, b.getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    // ambient neon glows
    {
        juce::ColourGradient gl (C::violet.withAlpha (0.20f), 120.0f, 40.0f, C::violet.withAlpha (0.0f), 520.0f, 40.0f, true);
        g.setGradientFill (gl);
        g.fillRect (b);
        juce::ColourGradient gr (C::cyan.withAlpha (0.12f), b.getWidth() - 100.0f, b.getHeight() - 80.0f,
                                 C::cyan.withAlpha (0.0f), b.getWidth() - 520.0f, b.getHeight() - 80.0f, true);
        g.setGradientFill (gr);
        g.fillRect (b);
    }

    // logo
    const juce::Rectangle<float> logo (20.0f, 20.0f, 50.0f, 50.0f);
    {
        g.setColour (C::violet.withAlpha (0.35f));
        g.fillRoundedRectangle (logo.expanded (3.0f), 15.0f);
        juce::ColourGradient lg (C::violet, logo.getX(), logo.getY(), C::cyan, logo.getRight(), logo.getBottom(), false);
        g.setGradientFill (lg);
        g.fillRoundedRectangle (logo, 13.0f);
        g.setColour (juce::Colour (0xff0b0d12));
        g.setFont (font (23.0f, true));
        g.drawText ("EZ", logo, juce::Justification::centred);
    }

    {
        juce::ColourGradient tg (juce::Colours::white, 84.0f, 0.0f, C::cyan.brighter (0.4f), 300.0f, 0.0f, false);
        g.setGradientFill (tg);
        g.setFont (font (30.0f, true));
        g.drawText ("Voice FX", 84, 16, 300, 34, juce::Justification::centredLeft);
        g.setColour (C::textDim);
        g.setFont (font (11.0f, true));
        g.drawText ("VOCAL CLEAN-UP SUITE   |   v1.0", 86, 50, 320, 16, juce::Justification::centredLeft);
    }

    g.setColour (C::textDim);
    g.setFont (font (10.5f, true));
    g.drawText (juce::String::fromUTF8 ("LOW CUT  \xe2\x80\xba  DE-CLICK  \xe2\x80\xba  AI DENOISE  \xe2\x80\xba  DE-PLOSIVE  \xe2\x80\xba  DE-REVERB"),
                420, 24, getWidth() - 440, 16, juce::Justification::centredRight);
    g.drawText (juce::String::fromUTF8 ("BREATH  \xe2\x80\xba  DE-ESS  \xe2\x80\xba  AUTO-LEVEL  \xe2\x80\xba  LIMITER"),
                420, 44, getWidth() - 440, 16, juce::Justification::centredRight);

    // bottom labels for output knob
    g.setColour (C::textDim);
    g.setFont (font (10.5f, true));
    g.drawText ("OUTPUT", getWidth() - 236, 604, 96, 14, juce::Justification::centred);
}

void EZVoiceFXAudioProcessorEditor::resized()
{
    const int margin = 20, gap = 14;
    const float cw = (float) (getWidth() - 2 * margin - 4 * gap) / 5.0f;
    const int rowH = 240, row1 = 92, row2 = row1 + rowH + gap;

    auto cell = [&] (int col, int span, int y)
    {
        const int x0 = margin + juce::roundToInt ((float) col * (cw + (float) gap));
        const int x1 = margin + juce::roundToInt ((float) (col + span) * (cw + (float) gap) - (float) gap);
        return juce::Rectangle<int> (x0, y, x1 - x0, rowH);
    };

    lowcut->setBounds    (cell (0, 1, row1));
    declick->setBounds   (cell (1, 1, row1));
    denoise->setBounds   (cell (2, 1, row1));
    deplosive->setBounds (cell (3, 1, row1));
    dereverb->setBounds  (cell (4, 1, row1));

    breath->setBounds  (cell (0, 1, row2));
    deess->setBounds   (cell (1, 1, row2));
    level->setBounds   (cell (2, 2, row2));
    limiter->setBounds (cell (4, 1, row2));

    const int barY = row2 + rowH + gap;           // 600
    const int barH = getHeight() - barY - 14;     // ~86
    meterPanel.setBounds (margin, barY, getWidth() - 2 * margin - 250, barH);
    outKnob.setBounds (getWidth() - 236, barY + 14, 96, barH - 12);
    bypassBtn.setBounds (getWidth() - margin - 112, barY + barH / 2 - 20, 112, 40);
}

void EZVoiceFXAudioProcessorEditor::timerCallback()
{
    auto& m = proc.meters;

    const int clicks = m.clicks.load();
    if (clicks != lastClicks) { clickFlash = 1.0f; lastClicks = clicks; }
    else clickFlash = juce::jmax (0.0f, clickFlash - 0.08f);

    declick->setMeterValue (clickFlash);
    denoise->setMeterValue (m.vad.load());
    deplosive->setMeterValue (m.plosiveGrDb.load() / 18.0f);
    dereverb->setMeterValue (0.0f);
    breath->setMeterValue (m.breathGrDb.load() / 20.0f);
    deess->setMeterValue (m.deessGrDb.load() / 15.0f);
    level->setMeterValue (m.compGrDb.load() / 6.0f);
    limiter->setMeterValue (m.limiterGrDb.load() / 8.0f);

    for (auto* c : { lowcut.get(), declick.get(), denoise.get(), deplosive.get(), dereverb.get(),
                     breath.get(), deess.get(), limiter.get() })
        c->refresh();
    level->refresh();
    level->refreshLearn();

    meterPanel.repaint();
}
