#include "PluginEditor.h"

using ezui::font;
static juce::String ru (const char* s) { return juce::String::fromUTF8 (s); }

//==============================================================================
MainUI::MainUI (EZVoiceFXAudioProcessor& p, ezui::EZLookAndFeel& l)
    : proc (p), lnf (l), presets (p.apvts), spectrum (p), history (p), meter (p), profileView (p), magic (p), beatPanel (p)
{
    auto& s = p.apvts;

    // ---- header ----
    presetBox.setTooltip (ru ("Пресеты: заводские и ваши"));
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        if (id < PresetManager::kUserIdBase) presets.loadFactory (id - 1);
        else presets.loadUser (presetBox.getItemText (presetBox.indexOfItemId (id)));
        proc.undoManager.beginNewTransaction();
    };
    addAndMakeVisible (presetBox);
    refreshPresetList();

    saveBtn.onClick = [this] { savePreset(); };
    undoBtn.onClick = [this] { proc.undoManager.undo(); };
    redoBtn.onClick = [this] { proc.undoManager.redo(); };
    gearBtn.onClick = [this] { showSettingsMenu(); };
    for (auto* b : { &saveBtn, &undoBtn, &redoBtn, &gearBtn }) addAndMakeVisible (*b);
    magicBtn.setToggleState (true, juce::dontSendNotification);     // drawn with the accent gradient

    // ---- sidebar ----
    ezui::SidebarTab* tabs[] = { &tabMain, &tabClean, &tabLevel, &tabRef, &tabAdv };
    for (int i = 0; i < kPages; ++i)
    {
        tabs[i]->onClick = [this, i] { selectPage (i); };
        addAndMakeVisible (*tabs[i]);
    }

    // ---- display + pages ----
    addAndMakeVisible (spectrum);
    addChildComponent (history);
    addChildComponent (profileView);
    addAndMakeVisible (meter);
    pages[0] = std::make_unique<ezui::MainPage> (p);
    pages[1] = std::make_unique<ezui::CleanPage> (p);
    pages[2] = std::make_unique<ezui::LevelPage> (p);
    pages[3] = std::make_unique<ezui::RefPage> (p);
    pages[4] = std::make_unique<ezui::AdvancedPage> (p);
    for (auto& pg : pages) addChildComponent (*pg);

    // ---- bottom bar ----
    for (auto* sl : { &mixSlider, &outSlider })
    {
        sl->setSliderStyle (juce::Slider::LinearHorizontal);
        sl->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (*sl);
    }
    mixSlider.setTooltip (ru ("Mix: смесь обработанного и исходного звука"));
    outSlider.setTooltip (ru ("Общая громкость (перед лимитером)"));
    mixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, ParamIDs::mix, mixSlider);
    outAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, ParamIDs::outputGain, outSlider);
    mixSlider.setDoubleClickReturnValue (true, 100.0);
    outSlider.setDoubleClickReturnValue (true, 0.0);

    listenBtn.setClickingTogglesState (true);
    powerBtn.setClickingTogglesState (true);
    addAndMakeVisible (listenBtn);
    addAndMakeVisible (powerBtn);
    listenAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, ParamIDs::listen, listenBtn);
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, ParamIDs::bypass, powerBtn);

    // ---- magic, beat link, welcome ----
    magicBtn.setButtonText ("MAGIC");
    magicBtn.setTooltip (ru ("Сделать магию: плагин послушает капу и бит и настроит всё сам"));
    magicBtn.setClickingTogglesState (false);
    magicBtn.onClick = [this] { startMagicFlow(); };
    addAndMakeVisible (magicBtn);

    addChildComponent (beatPanel);
    beatPanel.onMakeVocal = [this] { setRole (false); };

    addChildComponent (magic);
    magic.onCancel = [this] { proc.cancelMagic(); magic.setVisible (false); magicVisible = false; };
    magic.onFinish = [this] { proc.finishMagic(); };
    magic.onManual = [this] { proc.resetMagic(); magic.setVisible (false); magicVisible = false; selectPage (0); };
    magic.onAgain  = [this] { proc.startMagic(); };

    if (! (bool) proc.getUiProperty ("welcomeDone", false))
    {
        welcome = std::make_unique<ezui::WelcomeOverlay>();
        welcome->onMagic  = [this] { proc.setUiProperty ("welcomeDone", true); welcome->setVisible (false); startMagicFlow(); };
        welcome->onManual = [this] { proc.setUiProperty ("welcomeDone", true); welcome->setVisible (false); };
        welcome->onBeat   = [this] { proc.setUiProperty ("welcomeDone", true); welcome->setVisible (false); setRole (true); };
        addAndMakeVisible (*welcome);
    }

    selectPage ((int) proc.getUiProperty ("page", 0));
    updateRoleVisibility();
}

void MainUI::startMagicFlow()
{
    if (proc.isBeatRole()) setRole (false);
    proc.startMagic();
    magicVisible = true;
    magic.setVisible (true);
    magic.toFront (false);
}

void MainUI::setRole (bool beat)
{
    if (auto* prm = proc.apvts.getParameter (ParamIDs::role))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (beat ? 1.0f : 0.0f);
        prm->endChangeGesture();
    }
}

void MainUI::updateRoleVisibility()
{
    const bool beat = proc.isBeatRole();
    lastBeatRole = beat;
    beatPanel.setVisible (beat);
    for (auto* c : { (juce::Component*) &tabMain, (juce::Component*) &tabClean, (juce::Component*) &tabLevel,
                     (juce::Component*) &tabRef, (juce::Component*) &tabAdv, (juce::Component*) &spectrum,
                     (juce::Component*) &history, (juce::Component*) &profileView })
        c->setVisible (! beat);
    for (int i = 0; i < kPages; ++i) pages[i]->setVisible (! beat && i == currentPage);
    if (! beat)
    {
        history.setVisible (currentPage == 2);
        profileView.setVisible (currentPage == 3);
        spectrum.setVisible (currentPage != 2 && currentPage != 3);
    }
    listenBtn.setVisible (! beat);
    magicBtn.setVisible (! beat);
    repaint();
}

void MainUI::selectPage (int index)
{
    currentPage = juce::jlimit (0, kPages - 1, index);
    ezui::SidebarTab* tabs[] = { &tabMain, &tabClean, &tabLevel, &tabRef, &tabAdv };
    for (int i = 0; i < kPages; ++i)
    {
        tabs[i]->setToggleState (i == currentPage, juce::dontSendNotification);
        pages[i]->setVisible (i == currentPage);
    }
    history.setVisible (currentPage == 2);
    profileView.setVisible (currentPage == 3);
    spectrum.setVisible (currentPage != 2 && currentPage != 3);
    proc.setUiProperty ("page", currentPage);
}

void MainUI::refreshPresetList()
{
    presetBox.clear (juce::dontSendNotification);
    const auto fac = presets.getFactoryNames();
    for (int i = 0; i < fac.size(); ++i) presetBox.addItem (fac[i], i + 1);
    const auto user = presets.getUserNames();
    if (! user.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading (ru ("Мои пресеты"));
        for (int i = 0; i < user.size(); ++i) presetBox.addItem (user[i], PresetManager::kUserIdBase + i);
    }
    presetBox.setText (presets.getCurrentName(), juce::dontSendNotification);
    lastPresetName = presets.getCurrentName();
}

void MainUI::savePreset()
{
    auto* w = new juce::AlertWindow (ru ("Сохранить пресет"), ru ("Название пресета:"), juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", presets.getCurrentName());
    w->addButton (ru ("Сохранить"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (ru ("Отмена"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    addAndMakeVisible (w);
    w->setCentrePosition (getWidth() / 2, getHeight() / 2);

    juce::Component::SafePointer<MainUI> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result == 1 && safe != nullptr)
        {
            safe->presets.saveUser (w->getTextEditorContents ("name"));
            safe->refreshPresetList();
        }
    }), true);
}

void MainUI::showSettingsMenu()
{
    juce::PopupMenu themeMenu, sizeMenu, menu;
    const auto& all = ezui::themes();
    for (int i = 0; i < (int) all.size(); ++i)
        themeMenu.addItem (100 + i, all[(size_t) i].name, true, i == lnf.getThemeIndex());

    const float sizes[] = { 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };
    for (int i = 0; i < 5; ++i)
        sizeMenu.addItem (200 + i, juce::String (juce::roundToInt (sizes[i] * 100.0f)) + " %", true,
                          std::abs (sizes[i] - currentScale) < 0.01f);

    menu.addSectionHeader ("EZ Voice FX 1.3");
    menu.addSubMenu (ru ("Тема оформления"), themeMenu);
    menu.addSubMenu (ru ("Размер окна"), sizeMenu);
    menu.addSeparator();
    menu.addItem (1, ru ("Сбросить всё по умолчанию"));
    menu.addItem (2, ru ("Открыть папку пресетов"));

    juce::Component::SafePointer<MainUI> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&gearBtn), [safe, sizes] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r >= 100 && r < 200) { if (safe->onThemeChange) safe->onThemeChange (r - 100); }
        else if (r >= 200 && r < 205) { if (safe->onScaleChange) safe->onScaleChange (sizes[r - 200]); }
        else if (r == 1) { safe->presets.resetToDefault(); safe->refreshPresetList(); }
        else if (r == 2) PresetManager::getUserFolder().startAsProcess();
    });
}

//==============================================================================
void MainUI::resized()
{
    presetBox.setBounds (604, 24, 250, 34);
    saveBtn.setBounds (866, 23, 36, 36);
    undoBtn.setBounds (906, 23, 36, 36);
    redoBtn.setBounds (946, 23, 36, 36);
    gearBtn.setBounds (1030, 23, 40, 36);

    tabMain.setBounds (20, 104, 76, 74);
    tabClean.setBounds (20, 184, 76, 74);
    tabLevel.setBounds (20, 264, 76, 74);
    tabRef.setBounds (20, 344, 76, 74);
    tabAdv.setBounds (20, 424, 76, 74);

    const juce::Rectangle<int> display (126, 108, 864, 196);
    spectrum.setBounds (display);
    history.setBounds (display);
    profileView.setBounds (display);
    for (auto& pg : pages) pg->setBounds (112, 316, 892, 272);
    meter.setBounds (1024, 108, 60, 470);

    magicBtn.setBounds (464, 24, 120, 34);
    beatPanel.setBounds (14, 96, 990, 500);
    magic.setBounds (getLocalBounds());
    if (welcome != nullptr) welcome->setBounds (getLocalBounds());

    mixSlider.setBounds (176, 632, 250, 30);
    outSlider.setBounds (620, 632, 250, 30);
    listenBtn.setBounds (958, 627, 40, 40);
    powerBtn.setBounds (1014, 616, 62, 62);
}

void MainUI::paint (juce::Graphics& g)
{
    const auto& t = lnf.theme();
    const auto b = getLocalBounds().toFloat();

    juce::ColourGradient bg (t.bg1, 0.0f, 0.0f, t.bg2, b.getWidth(), b.getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();
    {
        juce::ColourGradient gl (t.glow.withAlpha (t.light ? 0.10f : 0.22f), 160.0f, 60.0f,
                                 t.glow.withAlpha (0.0f), 640.0f, 60.0f, true);
        g.setGradientFill (gl);
        g.fillRect (b);
        juce::ColourGradient gr (t.accent2.withAlpha (t.light ? 0.08f : 0.14f), b.getWidth() - 120.0f, b.getHeight() - 60.0f,
                                 t.accent2.withAlpha (0.0f), b.getWidth() - 620.0f, b.getHeight() - 60.0f, true);
        g.setGradientFill (gr);
        g.fillRect (b);
    }
    // glowing outer frame
    const auto frame = b.reduced (3.0f);
    if (! t.light)
    {
        g.setColour (t.accent.withAlpha (0.12f));
        g.drawRoundedRectangle (frame, 20.0f, 5.0f);
    }
    g.setColour (t.border);
    g.drawRoundedRectangle (frame, 20.0f, 1.4f);

    // logo + title
    ezui::drawWaveLogo (g, { 26.0f, 22.0f, 58.0f, 40.0f }, t);
    g.setColour (t.text);
    g.setFont (font (28.0f, true));
    g.drawText ("EZ Voice FX", 98, 16, 320, 34, juce::Justification::centredLeft);
    g.setColour (t.textDim);
    g.setFont (font (13.0f));
    g.drawText ("Professional Vocal Processor", 100, 48, 320, 18, juce::Justification::centredLeft);

    // engine badge
    {
        const juce::Rectangle<float> pill (300.0f, 26.0f, 72.0f, 22.0f);
        const bool pro = proc.isProEngineActive();
        juce::ColourGradient pg (t.accent, pill.getX(), pill.getY(), t.accent2, pill.getRight(), pill.getBottom(), false);
        g.setGradientFill (pg);
        g.fillRoundedRectangle (pill, 11.0f);
        g.setColour (t.light ? juce::Colours::white : juce::Colour (0xff0b0a12));
        g.setFont (font (11.5f, true));
        g.drawText (proc.isBeatRole() ? "BEAT" : (pro ? "AI PRO" : "AI LITE"), pill, juce::Justification::centred);
    }
    if (! proc.isBeatRole())
    {
        const int beats = proc.getLinkedBeats();
        g.setFont (font (10.5f, true));
        g.setColour (beats > 0 ? t.accent : t.textDim.withAlpha (0.7f));
        g.drawText (beats > 0 ? ru ("BEAT LINK: подключён") : ru ("BEAT LINK: нет"), 380, 6, 240, 14, juce::Justification::centred);
    }

    // panels
    ezui::drawPanel (g, { 14.0f, 96.0f, 88.0f, 500.0f }, t, 16.0f);          // sidebar
    ezui::drawPanel (g, { 112.0f, 96.0f, 892.0f, 500.0f }, t, 16.0f);         // main
    ezui::drawPanel (g, { 126.0f, 108.0f, 864.0f, 196.0f }, t, 12.0f);        // display
    ezui::drawPanel (g, { 1014.0f, 96.0f, 76.0f, 500.0f }, t, 16.0f);         // meter
    ezui::drawPanel (g, { 14.0f, 608.0f, 990.0f, 78.0f }, t, 16.0f);          // bottom

    // bottom labels
    const float mix = proc.apvts.getRawParameterValue (ParamIDs::mix)->load();
    const float out = proc.apvts.getRawParameterValue (ParamIDs::outputGain)->load();
    g.setFont (font (14.0f, true));
    g.setColour (t.text);
    g.drawText ("Mix", 124, 632, 50, 30, juce::Justification::centredLeft);
    g.drawText ("Output", 548, 632, 70, 30, juce::Justification::centredLeft);
    g.setFont (font (13.0f));
    g.setColour (t.textDim);
    g.drawText (juce::String (juce::roundToInt (mix)) + " %", 436, 632, 60, 30, juce::Justification::centredLeft);
    g.drawText ((out > 0.05f ? "+" : "") + juce::String (out, 1) + " dB", 880, 632, 70, 30, juce::Justification::centredLeft);
    g.setColour (t.border);
    g.drawVerticalLine (520, 624.0f, 670.0f);

    const float lufs = proc.meters.outLufs.load();
    g.setColour (t.textDim);
    g.setFont (font (10.5f, true));
    g.drawText (lufs < -60.0f ? juce::String ("-- LUFS") : juce::String (lufs, 1) + " LUFS", 1014, 580, 76, 14,
                juce::Justification::centred);
}

void MainUI::refresh()
{
    if (proc.isBeatRole() != lastBeatRole) updateRoleVisibility();
    const int ms = proc.getMagicState();
    if (ms != EZVoiceFXAudioProcessor::MagicIdle && ! magic.isVisible() && magicVisible) magic.setVisible (true);
    if (magic.isVisible()) magic.tick();
    if (beatPanel.isVisible()) beatPanel.refresh();
    const int beats = proc.getLinkedBeats();
    if (beats != lastBeats) { lastBeats = beats; repaint (380, 4, 240, 20); }

    if (spectrum.isVisible()) spectrum.update();
    if (history.isVisible()) history.repaint();
    if (profileView.isVisible()) profileView.repaint();
    if (currentPage != 3) proc.pollReference();          // results also arrive while another tab is open
    meter.repaint();
    pages[currentPage]->refresh();

    const auto name = presets.getCurrentName();
    if (name != lastPresetName)
    {
        lastPresetName = name;
        presetBox.setText (name, juce::dontSendNotification);
    }

    const float mix = proc.apvts.getRawParameterValue (ParamIDs::mix)->load();
    const float out = proc.apvts.getRawParameterValue (ParamIDs::outputGain)->load();
    const auto bottom = juce::String (mix, 0) + "|" + juce::String (out, 1) + "|" + juce::String (proc.meters.outLufs.load(), 1);
    if (bottom != lastBottomText) { lastBottomText = bottom; repaint (100, 600, 1000, 100); }

    const bool pro = proc.isProEngineActive();
    if (pro != lastPro) { lastPro = pro; repaint (290, 20, 100, 34); }
}

//==============================================================================
EZVoiceFXAudioProcessorEditor::EZVoiceFXAudioProcessorEditor (EZVoiceFXAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), ui (p, lnf)
{
    juce::PropertiesFile::Options opts;
    opts.applicationName = "EZ Voice FX";
    opts.filenameSuffix = ".settings";
    opts.folderName = "EZVibe";
    opts.osxLibrarySubFolder = "Application Support";
    settings = std::make_unique<juce::PropertiesFile> (opts);

    const int themeIdx = (int) proc.getUiProperty ("theme", settings->getIntValue ("theme", 0));
    const float scale = (float) (double) proc.getUiProperty ("scale", settings->getDoubleValue ("scale", 1.0));

    lnf.setTheme (themeIdx);
    setLookAndFeel (&lnf);
    addAndMakeVisible (ui);
    ui.sendLookAndFeelChange();
    ui.setBounds (0, 0, MainUI::kW, MainUI::kH);
    ui.onThemeChange = [this] (int i) { applyTheme (i); };
    ui.onScaleChange = [this] (float s) { applyScale (s); };

    setResizable (true, true);
    setResizeLimits (MainUI::kW * 6 / 10, MainUI::kH * 6 / 10, MainUI::kW * 2, MainUI::kH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) MainUI::kW / (double) MainUI::kH);
    ui.currentScale = juce::jlimit (0.6f, 2.0f, scale);
    setSize (juce::roundToInt (MainUI::kW * ui.currentScale), juce::roundToInt (MainUI::kH * ui.currentScale));
    startTimerHz (30);
}

EZVoiceFXAudioProcessorEditor::~EZVoiceFXAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void EZVoiceFXAudioProcessorEditor::applyTheme (int index)
{
    lnf.setTheme (index);
    proc.setUiProperty ("theme", index);
    settings->setValue ("theme", index);
    settings->saveIfNeeded();
    ui.sendLookAndFeelChange();
    ui.repaint();
}

void EZVoiceFXAudioProcessorEditor::applyScale (float s)
{
    ui.currentScale = s;
    proc.setUiProperty ("scale", (double) s);
    settings->setValue ("scale", (double) s);
    settings->saveIfNeeded();
    setSize (juce::roundToInt (MainUI::kW * s), juce::roundToInt (MainUI::kH * s));
}

void EZVoiceFXAudioProcessorEditor::resized()
{
    const float s = (float) getWidth() / (float) MainUI::kW;
    ui.setTransform (juce::AffineTransform::scale (s));
    ui.currentScale = s;
}

void EZVoiceFXAudioProcessorEditor::timerCallback()
{
    ui.refresh();
    if (++tick >= 15)
    {
        tick = 0;
        proc.undoManager.beginNewTransaction();
    }
}
