#pragma once

#include <duet/gui/Appearance.h>
#include <duet/gui/Tokens.h>

#include <juce_gui_basics/juce_gui_basics.h>

namespace duet::gui
{
/** A token's value as JUCE spells a colour. */
[[nodiscard]] inline juce::Colour toJuce (Colour value)
{
    return { value.red, value.green, value.blue, value.alpha };
}

/** The Graphite look, over the token set.

    One of these is the application's look and feel. It holds no colours of its
    own: every colour it hands a component is a token read under the palette the
    appearance says is in force, and it follows the appearance, so the producer
    switching theme repaints what is already on screen.

    What it changes about JUCE's default: the palette, Inter, scrollbars in the
    thin workstation form — no buttons, a thumb inset into a track that is not
    drawn — and flat surfaces, with a hairline border and a small radius carrying
    the shape that a gradient or a bevel would otherwise.
*/
class GraphiteLookAndFeel final : public juce::LookAndFeel_V4, private Appearance::Listener
{
public:
    explicit GraphiteLookAndFeel (Appearance& lookAndScale);
    ~GraphiteLookAndFeel() override;

    GraphiteLookAndFeel (const GraphiteLookAndFeel& other) = delete;
    GraphiteLookAndFeel& operator= (const GraphiteLookAndFeel& other) = delete;

    /** A token under the palette in force. */
    [[nodiscard]] juce::Colour colour (ColourToken token) const;

    //==============================================================================
    int getDefaultScrollbarWidth() override;
    int getScrollbarButtonSize (juce::ScrollBar& bar) override;
    void drawScrollbar (juce::Graphics& g,
                        juce::ScrollBar& bar,
                        int x,
                        int y,
                        int width,
                        int height,
                        bool isScrollbarVertical,
                        int thumbStartPosition,
                        int thumbSize,
                        bool isMouseOver,
                        bool isMouseDown) override;

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawAlertBox (juce::Graphics& g,
                       juce::AlertWindow& alert,
                       const juce::Rectangle<int>& textArea,
                       juce::TextLayout& textLayout) override;

    juce::Font getTextButtonFont (juce::TextButton& button, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label& label) override;
    juce::Font getComboBoxFont (juce::ComboBox& box) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;

private:
    void appearanceChanged() override;
    void applyPalette();
    static void refreshTextEditorColours (juce::Component& component, juce::Colour ink);

    Appearance& appearance;
};
} // namespace duet::gui
