#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ezui
{
namespace Colours
{
    const juce::Colour bgTop      { 0xff0c0e14 };
    const juce::Colour bgBottom   { 0xff140f24 };
    const juce::Colour card       { 0xff181b24 };
    const juce::Colour cardDark   { 0xff12141b };
    const juce::Colour track      { 0xff2a2e3a };
    const juce::Colour text       { 0xffeef0f6 };
    const juce::Colour textDim    { 0xff8a90a2 };
    const juce::Colour violet     { 0xff8b5cf6 };
    const juce::Colour cyan       { 0xff22d3ee };

    const juce::Colour lowcut     { 0xff4fc3f7 };
    const juce::Colour declick    { 0xffffb74d };
    const juce::Colour denoise    { 0xff9d7bff };
    const juce::Colour deplosive  { 0xffff7a50 };
    const juce::Colour dereverb   { 0xff26d0e0 };
    const juce::Colour breath     { 0xff7ed98a };
    const juce::Colour deess      { 0xfff06292 };
    const juce::Colour level      { 0xff00e5a0 };
    const juce::Colour limiter    { 0xffff5a5a };
    const juce::Colour output     { 0xffd0d4e0 };
}

inline juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size).withStyle (bold ? "Bold" : "Regular"));
}

//==============================================================================
class EZLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EZLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, Colours::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, Colours::violet.withAlpha (0.4f));
        setColour (juce::Label::textColourId, Colours::text);
        setColour (juce::ComboBox::backgroundColourId, Colours::cardDark);
        setColour (juce::ComboBox::outlineColourId, Colours::track);
        setColour (juce::ComboBox::textColourId, Colours::text);
        setColour (juce::ComboBox::arrowColourId, Colours::textDim);
        setColour (juce::PopupMenu::backgroundColourId, Colours::card);
        setColour (juce::PopupMenu::textColourId, Colours::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::violet.withAlpha (0.5f));
        setColour (juce::TextButton::textColourOffId, Colours::text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::TooltipWindow::backgroundColourId, Colours::card.brighter (0.1f));
        setColour (juce::TooltipWindow::textColourId, Colours::text);
        setColour (juce::TooltipWindow::outlineColourId, Colours::violet.withAlpha (0.6f));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float pos, float startAngle, float endAngle, juce::Slider& s) override
    {
        const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
        const auto b = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (8.0f);
        const float radius = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const float cx = b.getCentreX(), cy = b.getCentreY();
        const float lineW = juce::jmax (3.0f, radius * 0.13f);
        const float arcR = radius - lineW * 0.5f;
        const float angle = startAngle + pos * (endAngle - startAngle);

        // soft glow behind the knob
        {
            juce::ColourGradient glow (accent.withAlpha (0.22f), cx, cy,
                                       accent.withAlpha (0.0f), cx + radius * 1.25f, cy, true);
            g.setGradientFill (glow);
            g.fillEllipse (cx - radius * 1.25f, cy - radius * 1.25f, radius * 2.5f, radius * 2.5f);
        }

        // track
        juce::Path track;
        track.addCentredArc (cx, cy, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (Colours::track);
        g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // value arc with neon glow
        if (pos > 0.001f)
        {
            juce::Path val;
            val.addCentredArc (cx, cy, arcR, arcR, 0.0f, startAngle, angle, true);
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (accent.withAlpha (0.07f * (float) (4 - i)));
                g.strokePath (val, juce::PathStrokeType (lineW + (float) i * 4.0f,
                                                         juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            juce::ColourGradient grad (accent.brighter (0.5f), cx - radius, cy + radius,
                                       accent, cx + radius, cy - radius, false);
            g.setGradientFill (grad);
            g.strokePath (val, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // knob body
        const float kr = radius * 0.70f;
        {
            juce::ColourGradient body (juce::Colour (0xff343a48), cx, cy - kr,
                                       juce::Colour (0xff15181f), cx, cy + kr, false);
            g.setGradientFill (body);
            g.fillEllipse (cx - kr, cy - kr, kr * 2.0f, kr * 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.drawEllipse (cx - kr, cy - kr, kr * 2.0f, kr * 2.0f, 1.2f);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawEllipse (cx - kr - 1.5f, cy - kr - 1.5f, kr * 2.0f + 3.0f, kr * 2.0f + 3.0f, 1.5f);
        }

        // pointer
        {
            const float pr0 = kr * 0.35f, pr1 = kr * 0.82f;
            const juce::Point<float> p0 (cx + pr0 * std::sin (angle), cy - pr0 * std::cos (angle));
            const juce::Point<float> p1 (cx + pr1 * std::sin (angle), cy - pr1 * std::cos (angle));
            g.setColour (accent.withAlpha (0.35f));
            g.drawLine ({ p0, p1 }, 6.0f);
            g.setColour (accent.brighter (0.6f));
            g.drawLine ({ p0, p1 }, 2.6f);
        }
    }

    // Power-style toggle
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
    {
        const auto accent = b.findColour (juce::ToggleButton::tickColourId);
        const bool on = b.getToggleState();
        auto r = b.getLocalBounds().toFloat().reduced (2.0f);
        const float d = juce::jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (d, d);

        if (on)
        {
            g.setColour (accent.withAlpha (0.25f));
            g.fillEllipse (r.expanded (2.0f));
        }
        g.setColour (on ? accent.withAlpha (0.18f) : Colours::cardDark);
        g.fillEllipse (r);
        g.setColour (on ? accent : (highlighted ? Colours::textDim : Colours::track.brighter (0.3f)));
        g.drawEllipse (r, 1.4f);

        // power symbol
        const float cx = r.getCentreX(), cy = r.getCentreY(), pr = d * 0.26f;
        juce::Path p;
        p.addCentredArc (cx, cy, pr, pr, 0.0f, juce::MathConstants<float>::pi * 0.22f,
                         juce::MathConstants<float>::pi * 1.78f, true);
        g.setColour (on ? accent.brighter (0.4f) : Colours::textDim);
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawLine (cx, cy - pr * 1.25f, cx, cy - pr * 0.25f, 1.6f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const auto accent = b.findColour (juce::TextButton::buttonOnColourId);
        const bool on = b.getToggleState();
        juce::ColourGradient grad (on ? accent : Colours::card.brighter (0.15f), r.getX(), r.getY(),
                                   on ? accent.darker (0.5f) : Colours::cardDark, r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (on ? accent.brighter (0.3f) : (highlighted ? accent.withAlpha (0.8f) : accent.withAlpha (0.45f)));
        g.drawRoundedRectangle (r, 8.0f, down ? 2.0f : 1.3f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font (juce::jmin (14.0f, h * 0.45f), true); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.0f, true); }
    juce::Font getPopupMenuFont() override { return font (14.0f); }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (font (13.0f, true));
        l->setJustificationType (juce::Justification::centred);
        return l;
    }
};
} // namespace ezui
