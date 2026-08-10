// PROTOTYPE (r4m858) — Graphite theme tokens, ported from the mockup's app/globals.css.
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

struct Theme
{
    bool dark = true;

    juce::Colour canvas, surface, raised, interactive,
        borderDefault, borderSubtle,
        textPrimary, textSecondary, textMuted, textDisabled,
        accentStrong, accentDefault, accentBright, onAccent,
        info, success, warning, danger,
        gridFine, gridBeat, gridBar, meterTrack,
        keyBlack, keyWhite, overlayStrong, overlaySoft, shadow, scrollbarHover;

    juce::Colour trackColors[8];
    juce::Colour onTrack;

    // AI accent — TEAL, reserved for the Collaborator. Tunable live (HSB).
    float tealHue = 0, tealSat = 0, tealBri = 0;

    juce::Colour teal() const { return juce::Colour::fromHSV (tealHue, tealSat, tealBri, 1.0f); }

    void setTealFromHex (juce::uint32 argb)
    {
        auto c = juce::Colour (argb);
        tealHue = c.getHue(); tealSat = c.getSaturation(); tealBri = c.getBrightness();
    }

    static juce::Colour hex (juce::uint32 rgb) { return juce::Colour (0xff000000u | rgb); }

    static Theme darkTheme()
    {
        Theme t; t.dark = true;
        t.canvas = hex (0x0a0a0a); t.surface = hex (0x101010); t.raised = hex (0x181818); t.interactive = hex (0x212121);
        t.borderDefault = hex (0x282828); t.borderSubtle = hex (0x1c1c1c);
        // brightened one step from the mockup (#b8/#88/#6e/#4d) — dark-mode text read as too dim in the prototype
        t.textPrimary = hex (0xd2d2d2); t.textSecondary = hex (0x9e9e9e); t.textMuted = hex (0x828282); t.textDisabled = hex (0x5c5c5c);
        t.accentStrong = hex (0xc9c9c9); t.accentDefault = hex (0xdedede); t.accentBright = hex (0xf4f4f4); t.onAccent = hex (0x0a0a0a);
        t.info = hex (0x7ba7cc); t.success = hex (0x74b57f); t.warning = hex (0xcfa055); t.danger = hex (0xcc6560);
        juce::Colour tc[8] = { hex (0xab7c4c), hex (0xac6558), hex (0x5f9b81), hex (0x52909e),
                               hex (0xa09354), hex (0xa45555), hex (0x806fa3), hex (0x5b7ba5) };
        std::copy (tc, tc + 8, t.trackColors);
        t.onTrack = hex (0x0a0a0a);
        t.gridFine = juce::Colours::white.withAlpha (0.03f);
        t.gridBeat = juce::Colours::white.withAlpha (0.055f);
        t.gridBar  = juce::Colours::white.withAlpha (0.12f);
        t.meterTrack = juce::Colours::white.withAlpha (0.05f);
        t.keyBlack = hex (0x0e0e0e); t.keyWhite = hex (0x1d1d1d);
        t.overlayStrong = juce::Colours::black.withAlpha (0.42f);
        t.overlaySoft   = juce::Colours::black.withAlpha (0.22f);
        t.shadow = juce::Colours::black.withAlpha (0.68f);
        t.scrollbarHover = hex (0x313131);
        t.setTealFromHex (0xff4aa294); // muted from the grill's #3fd0be — neon/Tron on dark surfaces
        return t;
    }

    static Theme lightTheme()
    {
        Theme t; t.dark = false;
        t.canvas = hex (0xeeeeee); t.surface = hex (0xf8f8f8); t.raised = hex (0xffffff); t.interactive = hex (0xe3e3e3);
        t.borderDefault = hex (0xd5d5d5); t.borderSubtle = hex (0xe8e8e8);
        t.textPrimary = hex (0x1a1a1a); t.textSecondary = hex (0x4a4a4a); t.textMuted = hex (0x6b6b6b); t.textDisabled = hex (0x9a9a9a);
        t.accentStrong = hex (0x1f1f1f); t.accentDefault = hex (0x2e2e2e); t.accentBright = hex (0x454545); t.onAccent = hex (0xfafafa);
        t.info = hex (0x2c6f9e); t.success = hex (0x2d8340); t.warning = hex (0x96691a); t.danger = hex (0xac3e39);
        juce::Colour tc[8] = { hex (0x946420), hex (0x9c4c3f), hex (0x35785f), hex (0x2f6f7d),
                               hex (0x7d6a26), hex (0x94403f), hex (0x635383), hex (0x43618a) };
        std::copy (tc, tc + 8, t.trackColors);
        t.onTrack = hex (0xfbfbfb);
        t.gridFine = juce::Colours::black.withAlpha (0.045f);
        t.gridBeat = juce::Colours::black.withAlpha (0.075f);
        t.gridBar  = juce::Colours::black.withAlpha (0.15f);
        t.meterTrack = juce::Colours::black.withAlpha (0.08f);
        t.keyBlack = hex (0xe0e0e0); t.keyWhite = hex (0xfcfcfc);
        t.overlayStrong = juce::Colours::black.withAlpha (0.09f);
        t.overlaySoft   = juce::Colours::black.withAlpha (0.045f);
        t.shadow = juce::Colours::black.withAlpha (0.16f);
        t.scrollbarHover = hex (0xc4c4c4);
        t.setTealFromHex (0xff0e7c70);
        return t;
    }
};

// Minimal LookAndFeel bridge so stock widgets (combo, text editor, popup) follow the theme.
struct GraphiteLNF : juce::LookAndFeel_V4
{
    void apply (const Theme& t)
    {
        using namespace juce;
        setColour (ComboBox::backgroundColourId, t.interactive);
        setColour (ComboBox::textColourId, t.textPrimary);
        setColour (ComboBox::outlineColourId, t.borderDefault);
        setColour (ComboBox::arrowColourId, t.textMuted);
        setColour (PopupMenu::backgroundColourId, t.raised);
        setColour (PopupMenu::textColourId, t.textPrimary);
        setColour (PopupMenu::highlightedBackgroundColourId, t.interactive);
        setColour (PopupMenu::highlightedTextColourId, t.textPrimary);
        setColour (TextEditor::backgroundColourId, t.interactive);
        setColour (TextEditor::textColourId, t.textPrimary);
        setColour (TextEditor::outlineColourId, t.borderDefault);
        setColour (TextEditor::focusedOutlineColourId, t.accentStrong);
        setColour (TextEditor::highlightColourId, t.accentDefault.withAlpha (0.45f));
        setColour (CaretComponent::caretColourId, t.textPrimary);
        setColour (Label::textColourId, t.textPrimary);
        setColour (ScrollBar::thumbColourId, t.interactive);
        setColour (TooltipWindow::backgroundColourId, t.raised);
        setColour (TooltipWindow::textColourId, t.textPrimary);
        setColour (Slider::backgroundColourId, t.interactive);
        setColour (Slider::trackColourId, t.accentStrong);
        setColour (Slider::thumbColourId, t.accentDefault);
    }
};
