#include "Widgets.h"

namespace ezui
{
//==============================================================================
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const Theme& t, float radius)
{
    if (! t.light)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r.translated (0.0f, 3.0f), radius);
    }
    g.setColour (t.panel);
    g.fillRoundedRectangle (r, radius);
    juce::ColourGradient hl (juce::Colours::white.withAlpha (t.light ? 0.6f : 0.05f), r.getX(), r.getY(),
                             juce::Colours::white.withAlpha (0.0f), r.getX(), r.getY() + 60.0f, false);
    g.setGradientFill (hl);
    g.fillRoundedRectangle (r, radius);
    g.setColour (t.border.withAlpha (0.9f));
    g.drawRoundedRectangle (r, radius, 1.0f);
}

void drawWaveLogo (juce::Graphics& g, juce::Rectangle<float> r, const Theme& t)
{
    juce::Path p;
    const float x0 = r.getX(), w = r.getWidth(), cy = r.getCentreY(), h = r.getHeight();
    const float pts[] = { 0.0f, 0.0f, 0.12f, 0.0f, 0.22f, -0.18f, 0.32f, 0.30f, 0.42f, -0.45f, 0.52f, 0.42f,
                          0.62f, -0.30f, 0.72f, 0.16f, 0.82f, 0.0f, 1.0f, 0.0f };
    p.startNewSubPath (x0, cy);
    for (int i = 2; i < 20; i += 2) p.lineTo (x0 + pts[i] * w, cy + pts[i + 1] * h);
    if (! t.light)
    {
        g.setColour (t.accent.withAlpha (0.25f));
        g.strokePath (p, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    juce::ColourGradient grad (t.accent2, r.getX(), cy, t.accent, r.getRight(), cy, false);
    g.setGradientFill (grad);
    g.strokePath (p, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto& t = themeOf (*this);
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const float cx = r.getCentreX(), cy = r.getCentreY();

    if (icon == Icon::Power)
    {
        const bool active = ! getToggleState();          // toggle = bypassed
        const float d = juce::jmin (r.getWidth(), r.getHeight());
        auto c = r.withSizeKeepingCentre (d, d).reduced (3.0f);
        if (active && ! t.light)
        {
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (t.accent.withAlpha (0.08f * (float) (4 - i)));
                g.fillEllipse (c.expanded ((float) i * 3.0f));
            }
        }
        g.setColour (t.light ? juce::Colours::white : t.panel.withAlpha (1.0f));
        g.fillEllipse (c);
        g.setColour (active ? t.accent : t.textDim);
        g.drawEllipse (c, 2.0f);
        const float pr = c.getWidth() * 0.24f;
        juce::Path p;
        p.addCentredArc (cx, cy, pr, pr, 0.0f, juce::MathConstants<float>::pi * 0.22f,
                         juce::MathConstants<float>::pi * 1.78f, true);
        g.strokePath (p, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawLine (cx, cy - pr * 1.3f, cx, cy - pr * 0.15f, 2.4f);
        return;
    }

    if (highlighted || down || getToggleState())
    {
        g.setColour (getToggleState() ? t.accent.withAlpha (0.25f) : t.panel.withAlpha (1.0f));
        g.fillRoundedRectangle (r, 8.0f);
    }
    const auto col = getToggleState() ? t.accent : (highlighted ? t.text : t.textDim);
    g.setColour (col);
    const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.30f;
    const juce::PathStrokeType st (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    switch (icon)
    {
        case Icon::Save:
        {
            const juce::Rectangle<float> b (cx - s, cy - s, s * 2.0f, s * 2.0f);
            g.drawRoundedRectangle (b, 2.5f, 1.7f);
            g.drawRect (juce::Rectangle<float> (cx - s * 0.5f, cy - s, s, s * 0.6f), 1.5f);
            g.fillRect (juce::Rectangle<float> (cx - s * 0.55f, cy + s * 0.25f, s * 1.1f, s * 0.5f));
            break;
        }
        case Icon::Undo:
        case Icon::Redo:
        {
            const bool redo = icon == Icon::Redo;
            juce::Path p;
            const float a0 = redo ? -juce::MathConstants<float>::pi * 0.75f : juce::MathConstants<float>::pi * 0.75f;
            const float a1 = redo ? juce::MathConstants<float>::pi * 0.75f : -juce::MathConstants<float>::pi * 0.75f;
            p.addCentredArc (cx, cy, s, s, 0.0f, a0, a1, true);
            g.strokePath (p, st);
            const float ax = cx + s * std::sin (a0), ay = cy - s * std::cos (a0);
            const float dir = redo ? 1.0f : -1.0f;
            juce::Path head;
            head.addTriangle (ax - dir * s * 0.55f, ay - s * 0.05f, ax + dir * s * 0.25f, ay - s * 0.55f,
                              ax + dir * s * 0.25f, ay + s * 0.45f);
            g.fillPath (head);
            break;
        }
        case Icon::Gear:
        {
            juce::Path p;
            for (int i = 0; i < 8; ++i)
            {
                const float a = juce::MathConstants<float>::twoPi * (float) i / 8.0f;
                juce::Path tooth;
                tooth.addRoundedRectangle (-s * 0.18f, -s * 1.15f, s * 0.36f, s * 0.5f, 1.0f);
                p.addPath (tooth, juce::AffineTransform::rotation (a).translated (cx, cy));
            }
            g.fillPath (p);
            g.drawEllipse (cx - s * 0.8f, cy - s * 0.8f, s * 1.6f, s * 1.6f, 2.2f);
            g.drawEllipse (cx - s * 0.3f, cy - s * 0.3f, s * 0.6f, s * 0.6f, 1.6f);
            break;
        }
        case Icon::Headphones:
        {
            juce::Path p;
            p.addCentredArc (cx, cy + s * 0.2f, s, s, 0.0f, -juce::MathConstants<float>::halfPi,
                             juce::MathConstants<float>::halfPi, true);
            g.strokePath (p, st);
            g.fillRoundedRectangle (cx - s * 1.15f, cy + s * 0.1f, s * 0.5f, s * 0.9f, 2.0f);
            g.fillRoundedRectangle (cx + s * 0.65f, cy + s * 0.1f, s * 0.5f, s * 0.9f, 2.0f);
            break;
        }
        case Icon::Power: break;
    }
}

//==============================================================================
void SwitchToggle::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto& t = themeOf (*this);
    const bool on = getToggleState();
    const auto b = getLocalBounds().toFloat();
    const juce::Rectangle<float> sw (b.getX() + 2.0f, b.getCentreY() - 9.0f, 34.0f, 18.0f);
    g.setColour (on ? t.accent : t.track);
    g.fillRoundedRectangle (sw, 9.0f);
    if (highlighted) { g.setColour (t.accent.withAlpha (0.6f)); g.drawRoundedRectangle (sw, 9.0f, 1.0f); }
    const float kx = on ? sw.getRight() - 16.0f : sw.getX() + 2.0f;
    g.setColour (juce::Colours::white);
    g.fillEllipse (kx, sw.getY() + 2.0f, 14.0f, 14.0f);
    g.setColour (t.text);
    g.setFont (font (13.0f));
    g.drawText (getButtonText(), b.withTrimmedLeft (46.0f), juce::Justification::centredLeft);
}

//==============================================================================
void SidebarTab::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto& t = themeOf (*this);
    const bool sel = getToggleState();
    auto r = getLocalBounds().toFloat().reduced (3.0f);

    if (sel)
    {
        if (! t.light)
        {
            g.setColour (t.accent.withAlpha (0.25f));
            g.fillRoundedRectangle (r.expanded (2.0f), 12.0f);
        }
        juce::ColourGradient grad (t.accent.withAlpha (t.light ? 0.9f : 0.55f), r.getX(), r.getY(),
                                   t.accent2.withAlpha (t.light ? 0.9f : 0.35f), r.getRight(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (t.accent.brighter (0.3f));
        g.drawRoundedRectangle (r, 10.0f, 1.3f);
    }
    else if (highlighted)
    {
        g.setColour (t.panel.withAlpha (1.0f));
        g.fillRoundedRectangle (r, 10.0f);
    }

    const auto col = sel ? (t.light ? juce::Colours::white : t.text) : (highlighted ? t.text : t.textDim);
    g.setColour (col);
    const float cx = r.getCentreX(), cy = r.getY() + r.getHeight() * 0.38f, s = 11.0f;
    const juce::PathStrokeType st (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path p;
    switch (icon)
    {
        case Icon::Main:
            p.startNewSubPath (cx - s, cy);
            p.lineTo (cx - s * 0.5f, cy); p.lineTo (cx - s * 0.2f, cy - s * 0.8f); p.lineTo (cx + s * 0.2f, cy + s * 0.8f);
            p.lineTo (cx + s * 0.5f, cy - s * 0.3f); p.lineTo (cx + s, cy);
            g.strokePath (p, st);
            break;
        case Icon::Clean:
            for (int k = 0; k < 3; ++k)
            {
                juce::Path w;
                const float yy = cy - s * 0.6f + (float) k * s * 0.6f, amp = s * 0.22f * (float) (3 - k) / 3.0f;
                w.startNewSubPath (cx - s, yy);
                w.cubicTo (cx - s * 0.5f, yy - amp, cx - s * 0.1f, yy + amp, cx + s * 0.3f, yy);
                w.lineTo (cx + s, yy);
                g.strokePath (w, st);
            }
            break;
        case Icon::Level:
            g.fillRoundedRectangle (cx - s, cy - s * 0.2f, s * 0.45f, s, 1.5f);
            g.fillRoundedRectangle (cx - s * 0.22f, cy - s * 0.9f, s * 0.45f, s * 1.7f, 1.5f);
            g.fillRoundedRectangle (cx + s * 0.55f, cy - s * 0.5f, s * 0.45f, s * 1.3f, 1.5f);
            break;
        case Icon::Advanced:
            for (int i = 0; i < 8; ++i)
            {
                juce::Path tooth;
                tooth.addRoundedRectangle (-s * 0.17f, -s * 1.05f, s * 0.34f, s * 0.45f, 1.0f);
                p.addPath (tooth, juce::AffineTransform::rotation (juce::MathConstants<float>::twoPi * (float) i / 8.0f)
                                       .translated (cx, cy));
            }
            g.fillPath (p);
            g.drawEllipse (cx - s * 0.72f, cy - s * 0.72f, s * 1.44f, s * 1.44f, 2.0f);
            break;
    }
    g.setFont (font (12.5f, sel));
    g.drawText (getButtonText(), r.withTrimmedTop (r.getHeight() * 0.62f), juce::Justification::centredTop);
}

//==============================================================================
KnobCell::KnobCell (APVTS& s, const juce::String& t, const char* knobId, const char* powerId,
                    const juce::String& tooltip, const char* modeId,
                    const juce::String& modeText, const juce::String& modeTip)
    : title (t)
{
    if (modeId != nullptr)
    {
        hasMode = true;
        mode.setButtonText (modeText);
        mode.setTooltip (modeTip);
        mode.setClickingTogglesState (true);
        addAndMakeVisible (mode);
        modeAtt = std::make_unique<APVTS::ButtonAttachment> (s, modeId, mode);
    }

    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knob.setTooltip (tooltip);
    addAndMakeVisible (knob);
    knobAtt = std::make_unique<APVTS::SliderAttachment> (s, knobId, knob);
    if (auto* rp = s.getParameter (knobId))
        knob.setDoubleClickReturnValue (true, rp->convertFrom0to1 (rp->getDefaultValue()));

    if (powerId != nullptr)
    {
        hasPower = true;
        power.setTooltip (juce::String::fromUTF8 ("Включить / выключить"));
        addAndMakeVisible (power);
        powerAtt = std::make_unique<APVTS::ButtonAttachment> (s, powerId, power);
    }
}

void KnobCell::resized()
{
    const int w = getWidth(), h = getHeight();
    const int ks = juce::jmax (40, juce::jmin (w - 16, h - (hasMode ? 76 : 52)));
    knob.setBounds ((w - ks) / 2, 2, ks, ks);
    power.setBounds ((w + ks) / 2 - 12, 0, 24, 24);
    if (hasMode) mode.setBounds (w / 2 - 36, ks + 46, 72, 22);
}

void KnobCell::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    const int w = getWidth(), h = getHeight();
    const int ks = juce::jmax (40, juce::jmin (w - 16, h - (hasMode ? 76 : 52)));
    const bool on = ! hasPower || power.getToggleState();
    g.setColour (on ? t.text : t.textDim);
    g.setFont (font (14.0f, true));
    g.drawText (title, 0, ks + 6, w, 18, juce::Justification::centred);
    g.setColour (on ? t.text.withAlpha (0.85f) : t.textDim.withAlpha (0.6f));
    g.setFont (font (13.0f));
    g.drawText (knob.getTextFromValue (knob.getValue()), 0, ks + 25, w, 18, juce::Justification::centred);
}

void KnobCell::refresh()
{
    const bool on = ! hasPower || power.getToggleState();
    const auto txt = knob.getTextFromValue (knob.getValue());
    if (on != lastOn || txt != lastText)
    {
        lastOn = on; lastText = txt;
        knob.setAlpha (on ? 1.0f : 0.4f);
        repaint();
    }
}

//==============================================================================
SpectrumView::SpectrumView (EZVoiceFXAudioProcessor& p) : proc (p)
{
    fft.prepare (kN);
    window.resize ((size_t) kN);
    for (int i = 0; i < kN; ++i)
        window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * ez::kPi * i / kN));
    inRing.assign ((size_t) kN, 0.0f);
    outRing.assign ((size_t) kN, 0.0f);
    tmp.assign (16384, 0.0f);
    inDb.assign ((size_t) kPoints, -100.0f);
    outDb.assign ((size_t) kPoints, -100.0f);
    buf.resize ((size_t) kN);
}

static void appendToRing (std::vector<float>& ring, const float* data, int n)
{
    const int N = (int) ring.size();
    if (n >= N) { std::copy (data + (n - N), data + n, ring.begin()); return; }
    std::move (ring.begin() + n, ring.end(), ring.begin());
    std::copy (data, data + n, ring.end() - n);
}

void SpectrumView::update()
{
    int n = proc.inputTap.pull (tmp.data(), (int) tmp.size());
    if (n > 0) appendToRing (inRing, tmp.data(), n);
    n = proc.outputTap.pull (tmp.data(), (int) tmp.size());
    if (n > 0) appendToRing (outRing, tmp.data(), n);
    analyse (inRing, inDb);
    analyse (outRing, outDb);
    repaint();
}

void SpectrumView::analyse (const std::vector<float>& ring, std::vector<float>& db)
{
    for (int i = 0; i < kN; ++i) buf[(size_t) i] = std::complex<float> (ring[(size_t) i] * window[(size_t) i], 0.0f);
    fft.forward (buf.data());
    const double sr = proc.meters.sampleRate.load();
    const float binHz = (float) (sr / kN);
    const float norm = 2.0f / (0.5f * (float) kN);
    for (int p = 0; p < kPoints; ++p)
    {
        const float f = 20.0f * std::pow (1000.0f, (float) p / (float) (kPoints - 1));
        const int b0 = juce::jlimit (1, kN / 2 - 1, (int) std::floor (f * 0.944f / binHz));
        const int b1 = juce::jlimit (b0, kN / 2 - 1, (int) std::ceil (f * 1.059f / binHz));
        float m = 0.0f;
        for (int b = b0; b <= b1; ++b) m = std::max (m, std::abs (buf[(size_t) b]));
        const float v = 20.0f * std::log10 (m * norm + 1.0e-9f);
        float& s = db[(size_t) p];
        s = v > s ? v : std::max (v, s - 1.6f);
    }
}

float SpectrumView::xForFreq (float f) const
{
    return (float) getWidth() * std::log (f / 20.0f) / std::log (1000.0f);
}

float SpectrumView::yForDb (float d) const
{
    const float h = (float) getHeight();
    return juce::jlimit (0.0f, h, h * (-d) / 90.0f);
}

void SpectrumView::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    const float w = (float) getWidth(), h = (float) getHeight();

    // grid
    g.setFont (font (10.0f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xForFreq (f);
        g.setColour (t.border.withAlpha (0.35f));
        g.drawVerticalLine ((int) x, 0.0f, h);
        g.setColour (t.textDim.withAlpha (0.7f));
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                    (int) x + 3, (int) h - 14, 30, 12, juce::Justification::left);
    }
    for (float d : { -12.0f, -24.0f, -48.0f, -72.0f })
    {
        g.setColour (t.border.withAlpha (0.25f));
        g.drawHorizontalLine ((int) yForDb (d), 0.0f, w);
    }

    auto makePath = [&] (const std::vector<float>& db, bool closed)
    {
        juce::Path p;
        for (int i = 0; i < kPoints; ++i)
        {
            const float x = w * (float) i / (float) (kPoints - 1);
            const float y = yForDb (db[(size_t) i]);
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        if (closed) { p.lineTo (w, h); p.lineTo (0.0f, h); p.closeSubPath(); }
        return p;
    };

    // input (before) : soft fill
    g.setColour (t.textDim.withAlpha (t.light ? 0.18f : 0.14f));
    g.fillPath (makePath (inDb, true));
    g.setColour (t.textDim.withAlpha (0.35f));
    g.strokePath (makePath (inDb, false), juce::PathStrokeType (1.0f));

    // output (after) : glowing curve
    {
        juce::ColourGradient fillG (t.accent.withAlpha (t.light ? 0.30f : 0.35f), 0.0f, 0.0f,
                                    t.accent.withAlpha (0.0f), 0.0f, h, false);
        g.setGradientFill (fillG);
        g.fillPath (makePath (outDb, true));
        const auto line = makePath (outDb, false);
        if (! t.light)
        {
            g.setColour (t.accent.withAlpha (0.18f));
            g.strokePath (line, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        juce::ColourGradient lg (t.accent2, 0.0f, 0.0f, t.accent, w, 0.0f, false);
        g.setGradientFill (lg);
        g.strokePath (line, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // markers for module frequencies
    auto& s = proc.apvts;
    auto marker = [&] (float f, const juce::String& label)
    {
        if (f < 20.0f || f > 20000.0f) return;
        const float x = xForFreq (f);
        const int idx = juce::jlimit (0, kPoints - 1, juce::roundToInt ((float) (kPoints - 1) * x / w));
        const float y = juce::jlimit (12.0f, h - 18.0f, yForDb (outDb[(size_t) idx]));
        if (! t.light)
        {
            g.setColour (t.accent.withAlpha (0.3f));
            g.fillEllipse (x - 9.0f, y - 9.0f, 18.0f, 18.0f);
        }
        g.setColour (t.light ? t.accent : juce::Colours::white);
        g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f);
        g.setColour (t.text);
        g.setFont (font (10.5f, true));
        g.drawText (label, (int) x - 40, (int) y - 22, 80, 12, juce::Justification::centred);
    };
    if (s.getRawParameterValue (ParamIDs::lowcutOn)->load() > 0.5f)
        marker (s.getRawParameterValue (ParamIDs::lowcutFreq)->load(), "LOW CUT");
    if (s.getRawParameterValue (ParamIDs::humOn)->load() > 0.5f)
        marker (proc.meters.humBase.load(), "HUM");
    if (s.getRawParameterValue (ParamIDs::deessOn)->load() > 0.5f)
        marker (s.getRawParameterValue (ParamIDs::deessFreq)->load(), "DE-ESS");

    g.setColour (t.textDim);
    g.setFont (font (10.5f, true));
    g.drawText ("BEFORE", 8, 6, 60, 12, juce::Justification::left);
    g.setColour (t.accent);
    g.drawText ("AFTER", 62, 6, 60, 12, juce::Justification::left);
}

//==============================================================================
void HistoryView::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    const float w = (float) getWidth(), h = (float) getHeight();
    const float target = proc.apvts.getRawParameterValue (ParamIDs::levelTarget)->load();
    const float lo = target - 18.0f, hi = target + 9.0f;
    auto yFor = [&] (float v) { return juce::jlimit (0.0f, h, h * (hi - v) / (hi - lo)); };

    g.setColour (t.accent.withAlpha (0.10f));
    g.fillRect (juce::Rectangle<float> (0.0f, yFor (target + 1.0f), w, yFor (target - 1.0f) - yFor (target + 1.0f)));
    g.setColour (t.accent.withAlpha (0.6f));
    const float dash[] = { 6.0f, 5.0f };
    g.drawDashedLine (juce::Line<float> (0.0f, yFor (target), w, yFor (target)), dash, 2, 1.2f);

    g.setFont (font (10.5f));
    for (float v : { target + 6.0f, target - 6.0f, target - 12.0f })
    {
        g.setColour (t.border.withAlpha (0.3f));
        g.drawHorizontalLine ((int) yFor (v), 0.0f, w);
        g.setColour (t.textDim);
        g.drawText (juce::String (v, 0), 4, (int) yFor (v) - 13, 40, 12, juce::Justification::left);
    }

    const int N = EZVoiceFXAudioProcessor::Meters::kHistory;
    const int pos = proc.meters.historyPos.load();
    juce::Path p;
    bool started = false;
    for (int i = 0; i < N; ++i)
    {
        const float v = proc.meters.history[(pos + 1 + i) % N].load();
        const float x = w * (float) i / (float) (N - 1);
        if (v < -60.0f) { started = false; continue; }
        if (! started) { p.startNewSubPath (x, yFor (v)); started = true; }
        else p.lineTo (x, yFor (v));
    }
    if (! t.light)
    {
        g.setColour (t.accent.withAlpha (0.2f));
        g.strokePath (p, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    juce::ColourGradient lg (t.accent2, 0.0f, 0.0f, t.accent, w, 0.0f, false);
    g.setGradientFill (lg);
    g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (t.textDim);
    g.setFont (font (10.5f, true));
    g.drawText (juce::String::fromUTF8 ("ГРОМКОСТЬ НА ВЫХОДЕ, 30 С  |  ЦЕЛЬ ") + juce::String (target, 1) + " LUFS",
                8, 6, 400, 12, juce::Justification::left);
}

//==============================================================================
void OutputMeter::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    const float h = (float) getHeight() - 20.0f;
    auto yFor = [h] (float db) { return juce::jlimit (0.0f, h, h * (-db) / 60.0f); };

    const float pk = proc.meters.outPeakDb.load(), lu = proc.meters.outLufs.load();
    auto bar = [&] (float x, float valueDb)
    {
        const juce::Rectangle<float> tr (x, 0.0f, 9.0f, h);
        g.setColour (t.track);
        g.fillRoundedRectangle (tr, 4.5f);
        const float top = yFor (valueDb);
        if (top < h)
        {
            const auto fillR = tr.withTop (top);
            juce::ColourGradient grad (t.accent, 0.0f, 0.0f, t.accent2, 0.0f, h, false);
            if (valueDb > -1.0f) grad = juce::ColourGradient (juce::Colour (0xffff4d5e), 0.0f, 0.0f, t.accent2, 0.0f, h, false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (fillR, 4.5f);
        }
    };
    bar (4.0f, pk);
    bar (17.0f, lu);

    g.setFont (font (11.0f));
    g.setColour (t.textDim);
    for (float d : { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f })
        g.drawText (juce::String ((int) d), 30, (int) yFor (d) - 6, 28, 12, juce::Justification::left);
    g.setFont (font (9.5f, true));
    g.drawText ("PK", 0, (int) h + 4, 17, 12, juce::Justification::centred);
    g.drawText ("LU", 13, (int) h + 4, 17, 12, juce::Justification::centred);
}
} // namespace ezui
