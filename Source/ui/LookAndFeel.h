#pragma once

#include "Theme.h"

namespace ezui
{
inline juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size).withStyle (bold ? "Bold" : "Regular"));
}

//==============================================================================
class EZLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EZLookAndFeel() { setTheme (0); }

    void setTheme (int index)
    {
        themeIndex = juce::jlimit (0, (int) themes().size() - 1, index);
        const auto& t = theme();
        setColour (juce::Label::textColourId, t.text);
        setColour (juce::ComboBox::backgroundColourId, t.panel);
        setColour (juce::ComboBox::outlineColourId, t.border);
        setColour (juce::ComboBox::textColourId, t.text);
        setColour (juce::ComboBox::arrowColourId, t.textDim);
        setColour (juce::PopupMenu::backgroundColourId, t.light ? juce::Colours::white : t.bg2);
        setColour (juce::PopupMenu::textColourId, t.text);
        setColour (juce::PopupMenu::headerTextColourId, t.textDim);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, t.accent.withAlpha (0.35f));
        setColour (juce::PopupMenu::highlightedTextColourId, t.light ? t.text : juce::Colours::white);
        setColour (juce::TextButton::textColourOffId, t.text);
        setColour (juce::TextButton::textColourOnId, t.light ? juce::Colours::white : t.text);
        setColour (juce::TooltipWindow::backgroundColourId, t.light ? juce::Colours::white : t.bg2);
        setColour (juce::TooltipWindow::textColourId, t.text);
        setColour (juce::TooltipWindow::outlineColourId, t.accent.withAlpha (0.6f));
        setColour (juce::AlertWindow::backgroundColourId, t.light ? juce::Colours::white : t.bg2);
        setColour (juce::AlertWindow::textColourId, t.text);
        setColour (juce::AlertWindow::outlineColourId, t.border);
        setColour (juce::TextEditor::backgroundColourId, t.panel);
        setColour (juce::TextEditor::textColourId, t.text);
        setColour (juce::TextEditor::outlineColourId, t.border);
        setColour (juce::TextEditor::focusedOutlineColourId, t.accent);
        setColour (juce::ToggleButton::textColourId, t.text);
    }

    int getThemeIndex() const noexcept { return themeIndex; }
    const Theme& theme() const { return themes()[(size_t) themeIndex]; }

    //--------------------------------------------------------------------------
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float pos, float startAngle, float endAngle, juce::Slider& s) override
    {
        const auto& t = theme();
        const bool enabled = s.isEnabled() && s.getAlpha() > 0.9f;
        const auto b = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
        const float radius = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const float cx = b.getCentreX(), cy = b.getCentreY();
        const float lineW = juce::jmax (3.0f, radius * 0.11f);
        const float arcR = radius - lineW * 0.5f;
        const float angle = startAngle + pos * (endAngle - startAngle);
        const auto a1 = t.accent, a2 = t.accent2;

        if (! t.light)
        {
            juce::ColourGradient glow (t.glow.withAlpha (enabled ? 0.22f : 0.06f), cx, cy,
                                       t.glow.withAlpha (0.0f), cx + radius * 1.3f, cy, true);
            g.setGradientFill (glow);
            g.fillEllipse (cx - radius * 1.3f, cy - radius * 1.3f, radius * 2.6f, radius * 2.6f);
        }

        juce::Path track;
        track.addCentredArc (cx, cy, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (t.track);
        g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (pos > 0.001f)
        {
            juce::Path val;
            val.addCentredArc (cx, cy, arcR, arcR, 0.0f, startAngle, angle, true);
            if (! t.light)
                for (int i = 3; i >= 1; --i)
                {
                    g.setColour (a1.withAlpha (0.07f * (float) (4 - i)));
                    g.strokePath (val, juce::PathStrokeType (lineW + (float) i * 4.0f,
                                                             juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
            juce::ColourGradient grad (a2, cx - radius, cy + radius, a1, cx + radius, cy - radius, false);
            g.setGradientFill (grad);
            g.strokePath (val, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // body
        const float kr = radius * 0.72f;
        const juce::Rectangle<float> body (cx - kr, cy - kr, kr * 2.0f, kr * 2.0f);
        g.setColour (juce::Colours::black.withAlpha (t.light ? 0.12f : 0.55f));
        g.fillEllipse (body.translated (0.0f, kr * 0.12f).expanded (1.5f));

        if (t.knob == KnobStyle::Metal)
        {
            juce::ColourGradient metal (t.knobTop, cx - kr * 0.4f, cy - kr * 0.6f, t.knobBottom, cx + kr * 0.5f, cy + kr, true);
            g.setGradientFill (metal);
            g.fillEllipse (body);
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            for (int i = 0; i < 18; ++i)   // brushed rings
            {
                const float rr = kr * (0.15f + 0.045f * (float) i);
                g.drawEllipse (cx - rr, cy - rr, rr * 2.0f, rr * 2.0f, 0.6f);
            }
            g.setColour (a1.withAlpha (0.8f));
            g.drawEllipse (body, 1.6f);
        }
        else
        {
            juce::ColourGradient bodyG (t.knobTop, cx, cy - kr, t.knobBottom, cx, cy + kr, false);
            g.setGradientFill (bodyG);
            g.fillEllipse (body);
            g.setColour (t.light ? juce::Colours::white : juce::Colours::white.withAlpha (0.09f));
            g.drawEllipse (body.reduced (1.0f), 1.2f);
            if (t.light)
            {
                g.setColour (t.border);
                g.drawEllipse (body, 1.0f);
            }
        }

        // pointer
        const float pr0 = kr * 0.30f, pr1 = kr * 0.80f;
        const juce::Point<float> p0 (cx + pr0 * std::sin (angle), cy - pr0 * std::cos (angle));
        const juce::Point<float> p1 (cx + pr1 * std::sin (angle), cy - pr1 * std::cos (angle));
        const auto ptr = t.knob == KnobStyle::Metal ? juce::Colour (0xff2a2a2a)
                       : (t.light ? t.accent : a1.brighter (0.5f));
        if (t.knob == KnobStyle::Neon)
        {
            g.setColour (a1.withAlpha (0.35f));
            g.drawLine ({ p0, p1 }, 6.0f);
        }
        g.setColour (ptr);
        g.drawLine ({ p0, p1 }, 2.6f);
    }

    //--------------------------------------------------------------------------
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                           float, float, juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        if (style != juce::Slider::LinearHorizontal)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, 0.0f, 0.0f, style, s);
            return;
        }
        const auto& t = theme();
        const float cy = (float) y + (float) h * 0.5f;
        const juce::Rectangle<float> tr ((float) x, cy - 2.5f, (float) w, 5.0f);
        g.setColour (t.track);
        g.fillRoundedRectangle (tr, 2.5f);
        const auto fill = tr.withRight (sliderPos);
        juce::ColourGradient grad (t.accent2, tr.getX(), cy, t.accent, sliderPos, cy, false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, 2.5f);

        const float r = 8.0f;
        if (! t.light)
        {
            g.setColour (t.accent.withAlpha (0.3f));
            g.fillEllipse (sliderPos - r - 4.0f, cy - r - 4.0f, (r + 4.0f) * 2.0f, (r + 4.0f) * 2.0f);
        }
        g.setColour (t.light ? juce::Colours::white : t.accent.brighter (0.6f));
        g.fillEllipse (sliderPos - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour (t.accent);
        g.drawEllipse (sliderPos - r, cy - r, r * 2.0f, r * 2.0f, 2.0f);
    }

    //--------------------------------------------------------------------------
    // small power toggle (module on/off)
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
    {
        const auto& t = theme();
        const bool on = b.getToggleState();
        auto r = b.getLocalBounds().toFloat().reduced (2.0f);
        const float d = juce::jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (d, d);

        if (on && ! t.light)
        {
            g.setColour (t.accent.withAlpha (0.25f));
            g.fillEllipse (r.expanded (2.0f));
        }
        g.setColour (on ? t.accent.withAlpha (t.light ? 0.15f : 0.22f) : t.panel);
        g.fillEllipse (r);
        g.setColour (on ? t.accent : (highlighted ? t.textDim : t.border));
        g.drawEllipse (r, 1.3f);

        const float cx = r.getCentreX(), cy = r.getCentreY(), pr = d * 0.26f;
        juce::Path p;
        p.addCentredArc (cx, cy, pr, pr, 0.0f, juce::MathConstants<float>::pi * 0.22f,
                         juce::MathConstants<float>::pi * 1.78f, true);
        g.setColour (on ? t.accent : t.textDim);
        g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawLine (cx, cy - pr * 1.25f, cx, cy - pr * 0.2f, 1.5f);
    }

    //--------------------------------------------------------------------------
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        const auto& t = theme();
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        if (on)
        {
            juce::ColourGradient grad (t.accent, r.getX(), r.getY(), t.accent2, r.getRight(), r.getBottom(), false);
            g.setGradientFill (grad);
        }
        else
            g.setColour (t.panel.withAlpha (1.0f));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (on ? t.accent.brighter (0.3f) : (highlighted ? t.accent : t.border));
        g.drawRoundedRectangle (r, 8.0f, down ? 2.0f : 1.2f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const auto& t = theme();
        const juce::Rectangle<float> r (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f);
        g.setColour (t.light ? juce::Colours::white : t.panel.withAlpha (1.0f));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (box.hasKeyboardFocus (true) ? t.accent : t.border);
        g.drawRoundedRectangle (r, 8.0f, 1.2f);
        juce::Path arrow;
        const float ax = (float) w - 20.0f, ay = (float) h * 0.5f;
        arrow.startNewSubPath (ax - 5.0f, ay - 2.5f);
        arrow.lineTo (ax, ay + 2.5f);
        arrow.lineTo (ax + 5.0f, ay - 2.5f);
        g.setColour (t.textDim);
        g.strokePath (arrow, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (10, 1, box.getWidth() - 34, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.5f, true); }
    juce::Font getPopupMenuFont() override { return font (14.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font (juce::jmin (13.5f, (float) h * 0.45f), true); }
    juce::Font getAlertWindowMessageFont() override { return font (14.0f); }
    juce::Font getAlertWindowTitleFont() override { return font (17.0f, true); }

private:
    int themeIndex = 0;
};

inline const Theme& themeOf (juce::Component& c)
{
    if (auto* l = dynamic_cast<EZLookAndFeel*> (&c.getLookAndFeel())) return l->theme();
    return themes()[0];
}
} // namespace ezui
