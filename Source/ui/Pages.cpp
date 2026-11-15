#include "Pages.h"

namespace ezui
{
static juce::String ru (const char* s) { return juce::String::fromUTF8 (s); }

//==============================================================================
void Page::addCell (APVTS& s, const juce::String& title, const char* knob, const char* power, const juce::String& tip)
{
    cells.push_back (std::make_unique<KnobCell> (s, title, knob, power, tip));
    addAndMakeVisible (*cells.back());
}

void Page::layoutCells (juce::Rectangle<int> area)
{
    if (cells.empty()) return;
    const int w = area.getWidth() / (int) cells.size();
    for (size_t i = 0; i < cells.size(); ++i)
        cells[i]->setBounds (area.getX() + (int) i * w, area.getY(), w, area.getHeight());
}

void Page::resized()
{
    layoutCells (getLocalBounds().reduced (8, 6));
}

//==============================================================================
MainPage::MainPage (EZVoiceFXAudioProcessor& p)
{
    using namespace ParamIDs;
    auto& s = p.apvts;
    addCell (s, "AI Denoise", denoiseAmt, denoiseOn, ru ("Нейросеть убирает фоновый шум. 100% = максимальное подавление."));
    addCell (s, "De-Esser", deessAmt, deessOn, ru ("Приглушает резкие С, Ш, Щ только в момент их появления."));
    addCell (s, "De-Reverb", dereverbAmt, dereverbOn, ru ("Убирает отзвук комнаты. Больше 60% может звучать неестественно."));
    addCell (s, "VOX", voxAmt, voxOn, ru ("Одна ручка плотности, как Renaissance Vox: выравнивает и уплотняет голос, громкость при этом не меняется."));
    addCell (s, "Fit to Beat", fitAmt, fitOn, ru ("Врезает вокал в бит: убирает низ и бубнёж там, где бит плотный, добавляет разборчивости и воздуха. Лучше всего работает с Beat Link на бите."));
    addCell (s, "Auto-Level", levelTarget, levelOn, ru ("Целевая громкость каждой капы. -18 LUFS оставляет запас для дальнейшей обработки."));
}

void MainPage::refresh() { for (auto& c : cells) c->refresh(); }

//==============================================================================
CleanPage::CleanPage (EZVoiceFXAudioProcessor& p)
{
    using namespace ParamIDs;
    auto& s = p.apvts;
    addCell (s, "Low Cut", lowcutFreq, lowcutOn, ru ("Срезает гул и вибрации ниже выбранной частоты."));
    addCell (s, "Hum 50/60", humAmt, humOn, ru ("Убирает сетевой гул 50/60 Гц и его гармоники. Частота определяется автоматически."));
    addCell (s, "De-Click", declickAmt, declickOn, ru ("Убирает чмоки, щелчки рта и цифровые тики."));
    addCell (s, "De-Plosive", deplosiveAmt, deplosiveOn, ru ("Гасит удары воздуха на П и Б (с упреждением 2 мс)."));
    addCell (s, "Gate", gateAmt, gateOn, ru ("Приглушает остатки шума в паузах между фразами. Открывается по нейро-детектору голоса."));
    addCell (s, "Breath", breathAmt, breathOn, ru ("Делает вдохи тише (до -20 дБ), не вырезая их."));
}

void CleanPage::refresh() { for (auto& c : cells) c->refresh(); }

//==============================================================================
LevelPage::LevelPage (EZVoiceFXAudioProcessor& p)
    : proc (p), autoLearn (ru ("Замер по Play/Stop в DAW")), assist (ru ("Ассистент: подобрать настройки"))
{
    using namespace ParamIDs;
    auto& s = p.apvts;
    addCell (s, "Target", levelTarget, levelOn, ru ("Целевая громкость вокала, LUFS. Все капы приводятся к ней."));
    addCell (s, "Ceiling", limiterCeil, limiterOn, ru ("Потолок пиков лимитера (true peak)."));

    mode.addItemList ({ "Ride (auto)", "Learn (exact)" }, 1);
    addAndMakeVisible (mode);
    modeAtt = std::make_unique<APVTS::ComboBoxAttachment> (s, levelMode, mode);

    group.addItemList ({ ru ("Группа: выкл"), ru ("Группа A"), ru ("Группа B"), ru ("Группа C"), ru ("Группа D") }, 1);
    group.setTooltip (ru ("Экземпляры EZ Voice FX в одной группе анализируются вместе: нажали АНАЛИЗ в одном, замер идёт во всех. "
                          "Все капы группы придут к одной громкости. Если плагин стоит на шине группы, обычный АНАЛИЗ выравнивает всю шину."));
    addAndMakeVisible (group);
    groupAtt = std::make_unique<APVTS::ComboBoxAttachment> (s, linkGroup, group);

    learn.setButtonText (ru ("АНАЛИЗ (LEARN)"));
    learn.setTooltip (ru ("Нажмите, проиграйте капу целиком и нажмите ещё раз."));
    learn.onClick = [this]
    {
        if (proc.isLearning()) { proc.stopLearn(); return; }
        proc.startLearn();
        if (auto* prm = proc.apvts.getParameter (ParamIDs::levelMode))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (1.0f);
            prm->endChangeGesture();
        }
    };
    addAndMakeVisible (learn);

    autoLearn.setTooltip (ru ("Замер начинается при нажатии Play в DAW и заканчивается при Stop."));
    assist.setTooltip (ru ("Во время замера плагин анализирует капу и сам выставит Denoise, Gate, De-ess, De-plosive и Hum."));
    addAndMakeVisible (autoLearn);
    addAndMakeVisible (assist);
    autoAtt = std::make_unique<APVTS::ButtonAttachment> (s, learnAuto, autoLearn);
    assistAtt = std::make_unique<APVTS::ButtonAttachment> (s, assistant, assist);

    info.setJustificationType (juce::Justification::topLeft);
    info.setFont (font (12.5f));
    addAndMakeVisible (info);
    lookAndFeelChanged();
}

void LevelPage::lookAndFeelChanged()
{
    info.setColour (juce::Label::textColourId, themeOf (*this).textDim);
}

void LevelPage::resized()
{
    auto b = getLocalBounds().reduced (8, 6);
    auto left = b.removeFromLeft (b.getWidth() * 2 / 5);
    layoutCells (left);
    b.removeFromLeft (16);
    auto r = b.reduced (12, 10);
    auto row = r.removeFromTop (32);
    mode.setBounds (row.removeFromLeft (row.getWidth() / 2 - 6));
    row.removeFromLeft (12);
    learn.setBounds (row);
    r.removeFromTop (8);
    group.setBounds (r.removeFromTop (30).removeFromLeft (r.getWidth() / 2 - 6));
    r.removeFromTop (6);
    autoLearn.setBounds (r.removeFromTop (26));
    assist.setBounds (r.removeFromTop (26));
    r.removeFromTop (4);
    info.setBounds (r);
}

void LevelPage::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().reduced (8, 6);
    b.removeFromLeft (b.getWidth() * 2 / 5 + 16);
    drawPanel (g, b.toFloat(), themeOf (*this), 12.0f);
}

void LevelPage::refresh()
{
    for (auto& c : cells) c->refresh();
    const bool learning = proc.isLearning();
    learn.setToggleState (learning, juce::dontSendNotification);
    const bool grouped = proc.apvts.getRawParameterValue (ParamIDs::linkGroup)->load() >= 0.5f;
    learn.setButtonText (learning ? ru ("СТОП И ПРИМЕНИТЬ") : (grouped ? ru ("АНАЛИЗ ГРУППЫ") : ru ("АНАЛИЗ (LEARN)")));

    const bool learnMode = proc.apvts.getRawParameterValue (ParamIDs::levelMode)->load() >= 0.5f;
    const float target = proc.apvts.getRawParameterValue (ParamIDs::levelTarget)->load();
    const float gain = proc.meters.autoGainDb.load();

    juce::String txt;
    if (learning)
        txt = ru ("Идёт замер... Проиграйте капу целиком и нажмите СТОП.");
    else if (learnMode && proc.hasLearned())
    {
        const float lufs = proc.getLearnedLufs();
        const float g = juce::jlimit (-30.0f, 30.0f, target - lufs);
        txt = ru ("Замер: ") + juce::String (lufs, 1) + ru (" LUFS   Усиление: ") + (g >= 0.0f ? "+" : "")
              + juce::String (g, 1) + " dB";
    }
    else if (learnMode)
        txt = ru ("Нажмите АНАЛИЗ и проиграйте капу. Пока работает Ride.");
    else
        txt = ru ("Ride: громкость выравнивается автоматически. Усиление: ") + (gain >= 0.0f ? "+" : "")
              + juce::String (gain, 1) + " dB";

    const auto rep = proc.getAssistantReport();
    if (rep.isNotEmpty()) txt << "\n\n" << ru ("Ассистент:\n") << rep;
    if (info.getText() != txt) info.setText (txt, juce::dontSendNotification);
}

//==============================================================================
AdvancedPage::AdvancedPage (EZVoiceFXAudioProcessor& p)
    : proc (p), mono (ru ("Стерео в моно")), matchBypass (ru ("Bypass с выравниванием громкости")),
      whisper (ru ("Защита шёпота"))
{
    using namespace ParamIDs;
    auto& s = p.apvts;
    addCell (s, "De-Ess Freq", deessFreq, nullptr, ru ("Частота, выше которой работает де-эссер. Мужской голос 4-6 кГц, женский 6-8 кГц."));
    addCell (s, "Room Size", roomSize, nullptr, ru ("Время отзвука комнаты для дереверба. Маленькая комната 0.3 с, большая 1 с и больше."));

    engine.addItemList ({ ru ("Auto (по мощности ПК)"), "AI Pro (DeepFilterNet3)", ru ("AI Lite (RNNoise, эко)") }, 1);
    humFreq.addItemList ({ "Hum: Auto", "Hum: 50 Hz", "Hum: 60 Hz" }, 1);
    listen.addItemList ({ ru ("Слушать: результат"), ru ("Слушать: удалённое") }, 1);
    for (auto* c : { &engine, &humFreq, &listen }) addAndMakeVisible (*c);
    engine.onChange = [this] { engineChosen(); };
    humAtt = std::make_unique<APVTS::ComboBoxAttachment> (s, ParamIDs::humFreq, humFreq);
    listenAtt = std::make_unique<APVTS::ComboBoxAttachment> (s, ParamIDs::listen, listen);

    engine.setTooltip (ru ("Auto: плагин сам тестирует ПК и выбирает. AI Pro: DeepFilterNet3, лучшее качество. AI Lite: RNNoise, минимальная нагрузка."));
    whisper.setTooltip (ru ("Для шёпота и очень тихих фраз: нейросеть не «съедает» слова, гейт и дыхание работают мягче."));
    addAndMakeVisible (whisper);
    whisperAtt = std::make_unique<APVTS::ButtonAttachment> (s, ParamIDs::whisper, whisper);
    listen.setTooltip (ru ("«Удалённое» позволяет услышать, что именно убирает плагин."));
    addAndMakeVisible (mono);
    addAndMakeVisible (matchBypass);
    monoAtt = std::make_unique<APVTS::ButtonAttachment> (s, ParamIDs::mono, mono);
    matchAtt = std::make_unique<APVTS::ButtonAttachment> (s, ParamIDs::matchBypass, matchBypass);

    for (auto* l : { &status, &info, &hardware })
    {
        l->setJustificationType (juce::Justification::topLeft);
        l->setFont (font (12.5f));
        addAndMakeVisible (*l);
    }
    lookAndFeelChanged();
}

void AdvancedPage::lookAndFeelChanged()
{
    const auto& t = themeOf (*this);
    status.setColour (juce::Label::textColourId, t.accent);
    info.setColour (juce::Label::textColourId, t.textDim);
    hardware.setColour (juce::Label::textColourId, t.textDim);
}

void AdvancedPage::resized()
{
    auto b = getLocalBounds().reduced (8, 6);
    auto left = b.removeFromLeft (b.getWidth() * 2 / 5);
    layoutCells (left);
    b.removeFromLeft (16);
    auto r = b.reduced (12, 10);
    auto col1 = r.removeFromLeft (r.getWidth() / 2 - 8);
    r.removeFromLeft (16);
    auto col2 = r;

    engine.setBounds (col1.removeFromTop (30));
    col1.removeFromTop (4);
    status.setBounds (col1.removeFromTop (34));
    hardware.setBounds (col1.removeFromTop (70));
    humFreq.setBounds (col1.removeFromTop (30));
    col1.removeFromTop (6);
    listen.setBounds (col1.removeFromTop (30));

    whisper.setBounds (col2.removeFromTop (26));
    mono.setBounds (col2.removeFromTop (26));
    matchBypass.setBounds (col2.removeFromTop (26));
    col2.removeFromTop (6);
    info.setBounds (col2);
}

void AdvancedPage::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().reduced (8, 6);
    b.removeFromLeft (b.getWidth() * 2 / 5 + 16);
    drawPanel (g, b.toFloat(), themeOf (*this), 12.0f);
}

void AdvancedPage::setEngineParam (int index)
{
    if (auto* prm = proc.apvts.getParameter (ParamIDs::aiEngine))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) index));
        prm->endChangeGesture();
    }
}

void AdvancedPage::engineChosen()
{
    const int idx = engine.getSelectedId() - 1;
    if (idx < 0 || confirming) return;
    const int current = juce::roundToInt (proc.apvts.getRawParameterValue (ParamIDs::aiEngine)->load());
    if (idx == current) return;

    if (idx == 1 && ! proc.isProRecommended())
    {
        // weak PC (or test not finished): ask for an explicit confirmation
        confirming = true;
        engine.setSelectedId (current + 1, juce::dontSendNotification);
        auto* w = new juce::AlertWindow (ru ("Включить AI Pro?"),
            ru ("Тест показал, что этот компьютер может не справиться с AI Pro в реальном времени.\n\n"
                "Возможны щелчки, зависания и даже вылет DAW. Сохраните проект перед включением.\n\n"
                "Включить AI Pro всё равно?"),
            juce::MessageBoxIconType::WarningIcon);
        w->addButton (ru ("Включить, проект сохранён"), 1);
        w->addButton (ru ("Отмена"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
        auto* host = getParentComponent() != nullptr ? getParentComponent() : this;
        host->addAndMakeVisible (w);
        w->setCentrePosition (host->getWidth() / 2, host->getHeight() / 2);
        juce::Component::SafePointer<AdvancedPage> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int r)
        {
            if (safe == nullptr) return;
            safe->confirming = false;
            if (r == 1) safe->setEngineParam (1);
        }), true);
        return;
    }
    setEngineParam (idx);
}

void AdvancedPage::refresh()
{
    for (auto& c : cells) c->refresh();
    const int cur = juce::roundToInt (proc.apvts.getRawParameterValue (ParamIDs::aiEngine)->load());
    if (! confirming && engine.getSelectedId() != cur + 1) engine.setSelectedId (cur + 1, juce::dontSendNotification);
    const auto hw = proc.getHardwareText();
    if (hardware.getText() != hw) hardware.setText (hw, juce::dontSendNotification);
    const auto st = proc.getEngineStatus() + (proc.isProEngineActive() ? ru ("\nСейчас работает: AI Pro") : ru ("\nСейчас работает: AI Lite"));
    if (status.getText() != st) status.setText (st, juce::dontSendNotification);

    auto& m = proc.meters;
    const double sr = m.sampleRate.load();
    const double latMs = 1000.0 * m.latencySamples.load() / juce::jmax (1.0, sr);
    juce::String txt;
    txt << ru ("Задержка: ") << juce::String (latMs, 1) << ru (" мс (") << m.latencySamples.load() << ru (" сэмплов)\n")
        << ru ("Частота: ") << juce::String (sr / 1000.0, 1) << ru (" кГц\n")
        << ru ("Нагрузка CPU: ") << juce::roundToInt (m.cpu.load() * 100.0f) << " %\n"
        << ru ("Гул: ") << juce::roundToInt (m.humBase.load()) << ru (" Гц\n")
        << ru ("Голос (AI): ") << juce::roundToInt (m.vad.load() * 100.0f) << " %";
    if (info.getText() != txt) info.setText (txt, juce::dontSendNotification);
}
} // namespace ezui
