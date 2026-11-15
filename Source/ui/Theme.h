#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace ezui
{
enum class KnobStyle { Neon, Metal, Light };

struct Theme
{
    juce::String name;
    juce::Colour bg1, bg2;            // window gradient
    juce::Colour panel, border;       // panels
    juce::Colour accent, accent2;     // accent gradient
    juce::Colour text, textDim;
    juce::Colour track;               // knob / slider track
    juce::Colour knobTop, knobBottom; // knob body
    juce::Colour glow;                // ambient glow
    KnobStyle knob;
    bool light;
};

inline const std::vector<Theme>& themes()
{
    using C = juce::Colour;
    static const std::vector<Theme> t = {
        { "Neon Violet",
          C (0xff0b0a1f), C (0xff17113d), C (0xd9140f30), C (0xff3b2c86),
          C (0xff8b5cf6), C (0xff3b82f6), C (0xfff4f2ff), C (0xff9c95cc),
          C (0xff2a2452), C (0xff2e2858), C (0xff0d0a1e), C (0xff7c3aed), KnobStyle::Neon, false },
        { "Gold Luxe",
          C (0xff0c0b09), C (0xff1c1812), C (0xd916140f), C (0xff5c4b2a),
          C (0xffe5b45a), C (0xfff6d892), C (0xfff4ecda), C (0xffa89c80),
          C (0xff2d281e), C (0xffeeeeee), C (0xff5e5e5e), C (0xffc9963c), KnobStyle::Metal, false },
        { "Frost Light",
          C (0xffeef4fc), C (0xffd5e2f4), C (0xf2f7faff), C (0xffc3d4ec),
          C (0xff2f7cf6), C (0xff63b0ff), C (0xff1a2333), C (0xff62708a),
          C (0xffd4deee), C (0xffffffff), C (0xffc6d1e2), C (0xff5aa8ff), KnobStyle::Light, true },
        { "Midnight Studio",
          C (0xff07071a), C (0xff1d1147), C (0xd9100c2a), C (0xff35298a),
          C (0xff7c5cff), C (0xffc05cff), C (0xfff2efff), C (0xff958fc4),
          C (0xff26204e), C (0xff2a2552), C (0xff0a081a), C (0xff9b5cff), KnobStyle::Neon, false },
        { "Cyber Cyan",
          C (0xff040a10), C (0xff071b27), C (0xd9061420), C (0xff0f4d60),
          C (0xff00e5ff), C (0xff00a8c6), C (0xffe4fbff), C (0xff6ea4b2),
          C (0xff0e2a36), C (0xff1c2c36), C (0xff050b10), C (0xff00d0ff), KnobStyle::Neon, false },
        { "Black Gold",
          C (0xff070707), C (0xff16130c), C (0xd90f0e0b), C (0xff4b3e22),
          C (0xffd9a441), C (0xfff2cb70), C (0xfff3e9d3), C (0xff9e937b),
          C (0xff27231a), C (0xff2c2c2c), C (0xff040404), C (0xffb8862e), KnobStyle::Neon, false },
    };
    return t;
}
} // namespace ezui
