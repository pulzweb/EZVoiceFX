#include "Magic.h"

namespace ezui
{
static juce::String ru (const char* s) { return juce::String::fromUTF8 (s); }

static float bandToDisplay (float rms)
{
    const float db = 20.0f * std::log10 (rms + 1.0e-7f);
    return juce::jlimit (0.0f, 1.0f, (db + 70.0f) / 60.0f);
}

//==============================================================================
WelcomeOverlay::WelcomeOverlay() { setInterceptsMouseClicks (true, true); }

juce::Rectangle<float> WelcomeOverlay::card (int i) const
{
    const float w = 300.0f, h = 250.0f, gap = 30.0f;
    const float x0 = ((float) getWidth() - (3.0f * w + 2.0f * gap)) * 0.5f;
    return { x0 + (float) i * (w + gap), 250.0f, w, h };
}

void WelcomeOverlay::resized() {}

void WelcomeOverlay::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    g.fillAll ((t.light ? juce::Colours::white : juce::Colours::black).withAlpha (t.light ? 0.88f : 0.80f));

    g.setColour (t.text);
    g.setFont (font (34.0f, true));
    g.drawText (ru ("Как начнём?"), 0, 120, getWidth(), 44, juce::Justification::centred);
    g.setColour (t.textDim);
    g.setFont (font (15.0f));
    g.drawText (ru ("Это окно появляется один раз. Магию можно запустить в любой момент кнопкой MAGIC."),
                0, 170, getWidth(), 22, juce::Justification::centred);

    const juce::String titles[3] = { ru ("Сделать магию"), ru ("Ручное управление"), "Beat Link" };
    const juce::String texts[3] = {
        ru ("Включите воспроизведение проекта: плагин послушает капу и бит, всё настроит сам. Результат потом можно подправить вручную."),
        ru ("Сразу открыть все модули и ручки. Для тех, кто знает, что хочет."),
        ru ("Повесьте этот экземпляр на бит (минус). Вокальные экземпляры будут опираться на него и врезать вокал в бит.") };

    for (int i = 0; i < 3; ++i)
    {
        const auto r = card (i);
        const bool hov = hover == i;
        if (! t.light && (hov || i == 0))
        {
            g.setColour (t.accent.withAlpha (hov ? 0.35f : 0.18f));
            g.fillRoundedRectangle (r.expanded (6.0f), 22.0f);
        }
        if (i == 0)
        {
            juce::ColourGradient cg (t.accent.withAlpha (0.85f), r.getX(), r.getY(), t.accent2.withAlpha (0.85f),
                                     r.getRight(), r.getBottom(), false);
            g.setGradientFill (cg);
            g.fillRoundedRectangle (r, 18.0f);
        }
        else
        {
            drawPanel (g, r, t, 18.0f);
        }
        g.setColour (hov ? t.accent.brighter (0.4f) : t.border);
        g.drawRoundedRectangle (r, 18.0f, hov ? 2.0f : 1.0f);

        // icon
        const float cx = r.getCentreX(), cy = r.getY() + 70.0f;
        const auto ic = i == 0 ? juce::Colours::white : t.accent;
        g.setColour (ic);
        if (i == 0)
        {
            for (int k = 0; k < 4; ++k)          // sparkle
            {
                const float a = juce::MathConstants<float>::halfPi * (float) k;
                juce::Path ray;
                ray.addTriangle (cx, cy, cx + 6.0f * std::cos (a + 0.8f), cy + 6.0f * std::sin (a + 0.8f),
                                 cx + 30.0f * std::cos (a), cy + 30.0f * std::sin (a));
                ray.addTriangle (cx, cy, cx + 6.0f * std::cos (a - 0.8f), cy + 6.0f * std::sin (a - 0.8f),
                                 cx + 30.0f * std::cos (a), cy + 30.0f * std::sin (a));
                g.fillPath (ray);
            }
            g.fillEllipse (cx + 22.0f, cy - 32.0f, 9.0f, 9.0f);
        }
        else if (i == 1)
        {
            for (int k = 0; k < 3; ++k)          // sliders
            {
                const float y = cy - 18.0f + (float) k * 18.0f;
                g.drawLine (cx - 30.0f, y, cx + 30.0f, y, 2.5f);
                g.fillEllipse (cx - 30.0f + (float) ((k * 23) % 50) + 4.0f, y - 6.0f, 12.0f, 12.0f);
            }
        }
        else
        {
            juce::Path p;                        // beat waveform
            p.startNewSubPath (cx - 34.0f, cy);
            for (int k = 0; k <= 16; ++k)
                p.lineTo (cx - 34.0f + (float) k * 4.25f, cy + ((k % 4 == 0) ? -22.0f : (k % 2 ? 8.0f : -8.0f)));
            g.strokePath (p, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        g.setColour (i == 0 ? juce::Colours::white : t.text);
        g.setFont (font (21.0f, true));
        g.drawText (titles[i], r.withY (r.getY() + 112.0f).withHeight (28.0f), juce::Justification::centred);
        g.setColour (i == 0 ? juce::Colours::white.withAlpha (0.9f) : t.textDim);
        g.setFont (font (13.5f));
        g.drawFittedText (texts[i], r.reduced (22.0f, 0.0f).withY (r.getY() + 148.0f).withHeight (90.0f).toNearestInt(),
                          juce::Justification::centredTop, 5);
    }
}

void WelcomeOverlay::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < 3; ++i) if (card (i).contains (e.position)) h = i;
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void WelcomeOverlay::mouseExit (const juce::MouseEvent&) { hover = -1; repaint(); }

void WelcomeOverlay::mouseUp (const juce::MouseEvent& e)
{
    for (int i = 0; i < 3; ++i)
        if (card (i).contains (e.position))
        {
            if (i == 0 && onMagic) onMagic();
            if (i == 1 && onManual) onManual();
            if (i == 2 && onBeat) onBeat();
            return;
        }
}

//==============================================================================
MagicOverlay::MagicOverlay (EZVoiceFXAudioProcessor& p) : proc (p)
{
    cancelBtn.setButtonText (ru ("Отмена"));
    finishBtn.setButtonText (ru ("Готово, применить"));
    manualBtn.setButtonText (ru ("Открыть ручной режим"));
    againBtn.setButtonText (ru ("Ещё раз"));
    finishBtn.setClickingTogglesState (false);
    cancelBtn.onClick = [this] { if (onCancel) onCancel(); };
    finishBtn.onClick = [this] { if (onFinish) onFinish(); };
    manualBtn.onClick = [this] { if (onManual) onManual(); };
    againBtn.onClick = [this] { if (onAgain) onAgain(); };
    for (auto* b : { &cancelBtn, &finishBtn, &manualBtn, &againBtn }) addChildComponent (*b);
}

void MagicOverlay::resized()
{
    const int cx = getWidth() / 2, y = getHeight() - 86;
    cancelBtn.setBounds (cx - 230, y, 200, 44);
    finishBtn.setBounds (cx + 30, y, 200, 44);
    againBtn.setBounds (cx - 230, y, 200, 44);
    manualBtn.setBounds (cx + 30, y, 200, 44);
}

void MagicOverlay::tick()
{
    const int st = proc.getMagicState();
    phase += 0.035f;
    if (st == EZVoiceFXAudioProcessor::MagicAnalyzing) analyzeSpin += 0.12f;

    const float pk = proc.meters.outPeakDb.load();
    level += 0.25f * (juce::jlimit (0.0f, 1.0f, (pk + 50.0f) / 50.0f) - level);
    if (proc.meters.vad.load() > 0.5f && st == EZVoiceFXAudioProcessor::MagicListening) voiceSeen = true;
    if (proc.getLinkedBeats() > 0 && st == EZVoiceFXAudioProcessor::MagicListening) beatSeen = true;
    for (int b = 0; b < ez::LinkHub::kBands; ++b)
    {
        vocalBars[b] += 0.3f * (bandToDisplay (proc.meters.ownBands[b].load()) - vocalBars[b]);
        beatBars[b] += 0.3f * (bandToDisplay (proc.meters.otherBands[b].load()) - beatBars[b]);
    }

    if (st != lastState)
    {
        if (st == EZVoiceFXAudioProcessor::MagicWaiting) { voiceSeen = false; beatSeen = false; }
        lastState = st;
        const bool done = st == EZVoiceFXAudioProcessor::MagicDone;
        cancelBtn.setVisible (! done);
        finishBtn.setVisible (st == EZVoiceFXAudioProcessor::MagicListening);
        manualBtn.setVisible (done);
        againBtn.setVisible (done);
    }
    repaint();
}

void MagicOverlay::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    const int st = proc.getMagicState();
    const float W = (float) getWidth(), H = (float) getHeight();
    g.fillAll ((t.light ? t.bg1 : juce::Colours::black).withAlpha (t.light ? 0.94f : 0.86f));

    const float cx = W * 0.36f, cy = H * 0.46f, R = 120.0f;
    const bool listening = st == EZVoiceFXAudioProcessor::MagicListening;
    const bool done = st == EZVoiceFXAudioProcessor::MagicDone;
    const float lv = listening ? level : 0.15f + 0.05f * std::sin (phase * 2.0f);

    // aura
    for (int i = 6; i >= 1; --i)
    {
        const float rr = R * (1.0f + 0.12f * (float) i * (0.6f + lv));
        g.setColour (t.accent.withAlpha (0.035f * (float) (7 - i) * (0.5f + lv)));
        g.fillEllipse (cx - rr, cy - rr, rr * 2.0f, rr * 2.0f);
    }
    // core orb
    {
        juce::ColourGradient core (t.accent2.brighter (0.3f), cx - R * 0.3f, cy - R * 0.4f, t.accent.darker (0.6f), cx + R, cy + R, true);
        g.setGradientFill (core);
        g.fillEllipse (cx - R * 0.78f, cy - R * 0.78f, R * 1.56f, R * 1.56f);
    }
    // wavy ring reacting to the voice
    {
        juce::Path ring;
        for (int k = 0; k <= 180; ++k)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) k / 180.0f;
            const float wob = (6.0f + 26.0f * lv) * std::sin (a * 6.0f + phase * 3.0f) * std::sin (a * 3.0f - phase * 1.7f);
            const float rr = R * 0.92f + wob;
            const float x = cx + rr * std::cos (a), y = cy + rr * std::sin (a);
            if (k == 0) ring.startNewSubPath (x, y); else ring.lineTo (x, y);
        }
        ring.closeSubPath();
        g.setColour (t.accent.withAlpha (0.25f));
        g.strokePath (ring, juce::PathStrokeType (7.0f));
        juce::ColourGradient rg (t.accent2, cx - R, cy, t.accent, cx + R, cy, false);
        g.setGradientFill (rg);
        g.strokePath (ring, juce::PathStrokeType (2.2f));
    }
    // orbiting particles
    for (int k = 0; k < 24; ++k)
    {
        const float a = phase * (0.6f + 0.05f * (float) (k % 5)) + (float) k * 0.2618f;
        const float rr = R * (1.12f + 0.18f * std::sin (phase + (float) k));
        const float s = 2.0f + 3.0f * (0.5f + 0.5f * std::sin (phase * 2.0f + (float) k)) * (0.5f + lv);
        g.setColour ((k % 2 ? t.accent : t.accent2).withAlpha (0.8f));
        g.fillEllipse (cx + rr * std::cos (a) - s, cy + rr * std::sin (a) - s, s * 2.0f, s * 2.0f);
    }
    // progress arc (20 s is plenty)
    {
        const float prog = done ? 1.0f : juce::jlimit (0.0f, 1.0f, proc.getListenSeconds() / 20.0f);
        juce::Path bg, arc;
        bg.addCentredArc (cx, cy, R * 1.45f, R * 1.45f, 0.0f, 0.0f, juce::MathConstants<float>::twoPi, true);
        g.setColour (t.track.withAlpha (0.7f));
        g.strokePath (bg, juce::PathStrokeType (4.0f));
        if (st == EZVoiceFXAudioProcessor::MagicAnalyzing)
            arc.addCentredArc (cx, cy, R * 1.45f, R * 1.45f, 0.0f, analyzeSpin, analyzeSpin + 1.6f, true);
        else if (prog > 0.0f)
            arc.addCentredArc (cx, cy, R * 1.45f, R * 1.45f, 0.0f, 0.0f, juce::MathConstants<float>::twoPi * prog, true);
        g.setColour (t.accent);
        g.strokePath (arc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // centre text
    {
        juce::String big, small;
        switch (st)
        {
            case EZVoiceFXAudioProcessor::MagicWaiting:   big = ru ("Жду Play"); small = ru ("Включите воспроизведение проекта"); break;
            case EZVoiceFXAudioProcessor::MagicListening: big = juce::String (juce::roundToInt (proc.getListenSeconds())) + ru (" с"); small = ru ("Слушаю капу..."); break;
            case EZVoiceFXAudioProcessor::MagicAnalyzing: big = ru ("Колдую"); small = ru ("Подбираю настройки..."); break;
            case EZVoiceFXAudioProcessor::MagicDone:      big = ru ("Готово"); small = ru ("Всё настроено"); break;
            default: break;
        }
        g.setColour (juce::Colours::white);
        g.setFont (font (34.0f, true));
        g.drawText (big, (int) (cx - R), (int) cy - 30, (int) (R * 2.0f), 40, juce::Justification::centred);
        g.setFont (font (13.0f));
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawText (small, (int) (cx - R), (int) cy + 12, (int) (R * 2.0f), 20, juce::Justification::centred);
    }
    // spectrum bars: voice (up) vs beat (down)
    {
        const float bx = cx - 170.0f, by = cy + R * 1.62f + 20.0f, bw = 340.0f, bh = 40.0f;
        const float colW = bw / (float) ez::LinkHub::kBands;
        for (int b = 0; b < ez::LinkHub::kBands; ++b)
        {
            const float x = bx + (float) b * colW + 6.0f;
            g.setColour (t.accent);
            g.fillRoundedRectangle (x, by - bh * vocalBars[b], colW - 12.0f, bh * vocalBars[b] + 1.0f, 3.0f);
            g.setColour (t.accent2.withAlpha (0.7f));
            g.fillRoundedRectangle (x, by + 4.0f, colW - 12.0f, bh * beatBars[b] + 1.0f, 3.0f);
        }
        g.setColour (t.textDim);
        g.setFont (font (10.5f, true));
        g.drawText (ru ("ГОЛОС"), (int) bx - 60, (int) (by - 20.0f), 54, 14, juce::Justification::right);
        g.drawText (ru ("БИТ"), (int) bx - 60, (int) (by + 8.0f), 54, 14, juce::Justification::right);
    }

    // right column: checklist or summary
    const juce::Rectangle<float> panel (W * 0.62f, 110.0f, W * 0.34f, H - 230.0f);
    drawPanel (g, panel, t, 16.0f);
    g.setColour (t.text);
    g.setFont (font (18.0f, true));
    g.drawText (done ? ru ("Что сделано") : ru ("Анализ"), panel.reduced (20.0f, 14.0f).withHeight (26.0f).toNearestInt(),
                juce::Justification::left);

    if (done)
    {
        g.setColour (t.text);
        g.setFont (font (13.5f));
        g.drawFittedText (proc.getMagicSummary(), panel.reduced (20.0f).withTrimmedTop (40.0f).toNearestInt(),
                          juce::Justification::topLeft, 20);
    }
    else
    {
        const float secs = proc.getListenSeconds();
        const std::pair<juce::String, bool> items[] = {
            { ru ("Голос найден"), voiceSeen },
            { proc.getLinkedBeats() > 0 || beatSeen ? ru ("Бит найден (Beat Link)") : ru ("Бит: Beat Link не найден"), beatSeen },
            { ru ("Уровень шума"), secs > 3.0f },
            { ru ("Громкость капы"), secs > 5.0f },
            { ru ("Сибилянты и взрывные"), secs > 7.0f },
            { ru ("Динамика и шёпот"), secs > 9.0f },
            { ru ("Баланс с битом"), secs > 11.0f } };
        float y = panel.getY() + 56.0f;
        for (const auto& it : items)
        {
            const juce::Rectangle<float> box (panel.getX() + 22.0f, y + 2.0f, 18.0f, 18.0f);
            g.setColour (it.second ? t.accent : t.track);
            g.fillRoundedRectangle (box, 5.0f);
            if (it.second)
            {
                juce::Path tick;
                tick.startNewSubPath (box.getX() + 4.0f, box.getCentreY());
                tick.lineTo (box.getX() + 8.0f, box.getBottom() - 4.5f);
                tick.lineTo (box.getRight() - 3.5f, box.getY() + 4.0f);
                g.setColour (juce::Colours::white);
                g.strokePath (tick, juce::PathStrokeType (2.2f));
            }
            g.setColour (it.second ? t.text : t.textDim);
            g.setFont (font (14.0f, it.second));
            g.drawText (it.first, (int) box.getRight() + 12, (int) y, (int) panel.getWidth() - 70, 22, juce::Justification::left);
            y += 34.0f;
        }
        g.setColour (t.textDim);
        g.setFont (font (12.0f));
        g.drawFittedText (ru ("Совет: проиграйте 15-30 секунд самой громкой и самой тихой части капы. "
                              "Для «врезания в бит» повесьте EZ Voice FX на бит и выберите Beat Link."),
                          panel.reduced (20.0f).withTop (panel.getBottom() - 86.0f).toNearestInt(), juce::Justification::topLeft, 4);
    }
}

//==============================================================================
BeatPanel::BeatPanel (EZVoiceFXAudioProcessor& p)
    : proc (p),
      unmaskKnob (p.apvts, "Unmask", ParamIDs::unmaskAmt, nullptr,
                  ru ("Насколько бит уступает место голосу на частотах разборчивости (600 Гц - 5 кГц), только когда голос звучит."))
{
    addAndMakeVisible (unmaskKnob);
    vocalBtn.setButtonText (ru ("Сделать этот экземпляр вокальным"));
    vocalBtn.onClick = [this] { if (onMakeVocal) onMakeVocal(); };
    addAndMakeVisible (vocalBtn);
}

void BeatPanel::resized()
{
    unmaskKnob.setBounds (60, 250, 180, 190);
    vocalBtn.setBounds (getWidth() - 330, getHeight() - 64, 300, 40);
}

void BeatPanel::refresh()
{
    for (int b = 0; b < ez::LinkHub::kBands; ++b)
    {
        own[b] += 0.3f * (bandToDisplay (proc.meters.ownBands[b].load()) - own[b]);
        other[b] += 0.3f * (bandToDisplay (proc.meters.otherBands[b].load()) - other[b]);
    }
    unmaskKnob.refresh();
    repaint();
}

void BeatPanel::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    drawPanel (g, getLocalBounds().toFloat(), t, 16.0f);

    g.setColour (t.text);
    g.setFont (font (30.0f, true));
    g.drawText ("BEAT LINK", 40, 30, 400, 40, juce::Justification::left);
    g.setColour (t.textDim);
    g.setFont (font (14.0f));
    g.drawFittedText (ru ("Этот экземпляр висит на бите. Он сообщает вокальным экземплярам EZ Voice FX, где бит плотный, "
                          "и сам слегка уступает место голосу."), 40, 74, getWidth() - 80, 40, juce::Justification::topLeft, 2);

    const int vocals = proc.getLinkedVocals();
    g.setColour (vocals > 0 ? t.accent : t.textDim);
    g.setFont (font (16.0f, true));
    g.drawText (vocals > 0 ? ru ("Связано вокалов: ") + juce::String (vocals) : ru ("Вокальные экземпляры пока не найдены"),
                40, 130, 500, 24, juce::Justification::left);

    // bars: beat (own) and vocal (other) per band
    const juce::Rectangle<float> area (300.0f, 170.0f, (float) getWidth() - 360.0f, 240.0f);
    const char* labels[] = { "60", "150", "300", "600", "1.2k", "2.5k", "5k", "10k" };
    const float colW = area.getWidth() / (float) ez::LinkHub::kBands;
    for (int b = 0; b < ez::LinkHub::kBands; ++b)
    {
        const float x = area.getX() + (float) b * colW;
        const float hb = area.getHeight() * own[b], hv = area.getHeight() * other[b];
        g.setColour (t.accent2.withAlpha (0.55f));
        g.fillRoundedRectangle (x + 8.0f, area.getBottom() - hb, colW * 0.4f, hb, 4.0f);
        g.setColour (t.accent);
        g.fillRoundedRectangle (x + 8.0f + colW * 0.42f, area.getBottom() - hv, colW * 0.4f, hv, 4.0f);
        g.setColour (t.textDim);
        g.setFont (font (11.0f));
        g.drawText (labels[b], (int) x, (int) area.getBottom() + 6, (int) colW, 14, juce::Justification::centred);
        if (b >= 3 && b <= 6)
        {
            const float gdb = proc.meters.eqGains[b - 3].load();
            g.setColour (gdb < -0.5f ? t.accent : t.textDim);
            g.drawText (juce::String (gdb, 1) + " dB", (int) x, (int) area.getY() - 18, (int) colW, 14, juce::Justification::centred);
        }
    }
    g.setFont (font (11.5f, true));
    g.setColour (t.accent2);
    g.drawText (ru ("БИТ"), (int) area.getX(), (int) area.getBottom() + 26, 60, 14, juce::Justification::left);
    g.setColour (t.accent);
    g.drawText (ru ("ГОЛОС"), (int) area.getX() + 60, (int) area.getBottom() + 26, 80, 14, juce::Justification::left);
}
} // namespace ezui
