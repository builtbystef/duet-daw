#include <duet/gui/GraphiteLookAndFeel.h>

#include <duet/gui/Typography.h>

namespace duet::gui
{
GraphiteLookAndFeel::GraphiteLookAndFeel (Appearance& lookAndScale) : appearance (lookAndScale)
{
    // The application typeface, and not only the fonts this class hands out
    // below: anything JUCE draws with the default sans-serif — a tab's label, a
    // slider's text box, a tooltip — is Inter too.
    setDefaultSansSerifTypeface (interRegular());

    appearance.addListener (this);
    applyPalette();
}

GraphiteLookAndFeel::~GraphiteLookAndFeel() { appearance.removeListener (this); }

juce::Colour GraphiteLookAndFeel::colour (ColourToken token) const
{
    return toJuce (appearance.colour (token));
}

void GraphiteLookAndFeel::appearanceChanged() { applyPalette(); }

void GraphiteLookAndFeel::applyPalette()
{
    const auto canvas = colour (ColourToken::surfaceCanvas);
    const auto surface = colour (ColourToken::surfaceDefault);
    const auto raised = colour (ColourToken::surfaceRaised);
    const auto interactive = colour (ColourToken::surfaceInteractive);
    const auto hairline = colour (ColourToken::borderDefault);
    const auto ink = colour (ColourToken::textPrimary);
    const auto muted = colour (ColourToken::textMuted);
    const auto disabled = colour (ColourToken::textDisabled);
    const auto accent = colour (ColourToken::accentDefault);
    const auto onAccent = colour (ColourToken::onAccent);

    setColour (juce::ResizableWindow::backgroundColourId, canvas);
    setColour (juce::DocumentWindow::textColourId, ink);

    setColour (juce::Label::textColourId, ink);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);

    setColour (juce::TextButton::buttonColourId, interactive);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, ink);
    setColour (juce::TextButton::textColourOnId, onAccent);
    setColour (juce::ToggleButton::textColourId, ink);
    setColour (juce::ToggleButton::tickColourId, accent);
    setColour (juce::ToggleButton::tickDisabledColourId, disabled);
    setColour (juce::ComboBox::backgroundColourId, interactive);
    setColour (juce::ComboBox::textColourId, ink);
    setColour (juce::ComboBox::outlineColourId, hairline);
    setColour (juce::ComboBox::arrowColourId, muted);
    setColour (juce::ComboBox::buttonColourId, interactive);

    setColour (juce::PopupMenu::backgroundColourId, raised);
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, interactive);
    setColour (juce::PopupMenu::highlightedTextColourId, ink);

    setColour (juce::TextEditor::backgroundColourId, interactive);
    setColour (juce::TextEditor::textColourId, ink);
    setColour (juce::TextEditor::outlineColourId, hairline);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.45F));
    setColour (juce::CaretComponent::caretColourId, ink);

    setColour (juce::Slider::backgroundColourId, colour (ColourToken::meterTrack));
    setColour (juce::Slider::trackColourId, accent);
    setColour (juce::Slider::thumbColourId, accent);
    setColour (juce::Slider::textBoxTextColourId, ink);
    setColour (juce::Slider::textBoxBackgroundColourId, interactive);
    setColour (juce::Slider::textBoxOutlineColourId, hairline);

    // The track is not drawn: a workstation scrollbar is a thumb over whatever
    // it is scrolling, and the space it would take is the gap around the thumb.
    setColour (juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, interactive);

    setColour (juce::TabbedComponent::backgroundColourId, surface);
    setColour (juce::TabbedComponent::outlineColourId, hairline);
    setColour (juce::TabbedButtonBar::tabOutlineColourId, hairline);
    setColour (juce::TabbedButtonBar::tabTextColourId, muted);
    setColour (juce::TabbedButtonBar::frontOutlineColourId, hairline);
    setColour (juce::TabbedButtonBar::frontTextColourId, ink);

    setColour (juce::GroupComponent::outlineColourId, hairline);
    setColour (juce::GroupComponent::textColourId, muted);
    setColour (juce::TooltipWindow::backgroundColourId, raised);
    setColour (juce::TooltipWindow::textColourId, ink);
    setColour (juce::TooltipWindow::outlineColourId, hairline);
}

int GraphiteLookAndFeel::getDefaultScrollbarWidth()
{
    return appearance.scaled (metrics::scrollbarThickness);
}

int GraphiteLookAndFeel::getScrollbarButtonSize (juce::ScrollBar& /*bar*/)
{
    // No end buttons. A workstation scrollbar is dragged or scrolled past, never
    // stepped, and the buttons are the widest part of JUCE's default one.
    return 0;
}

void GraphiteLookAndFeel::drawScrollbar (juce::Graphics& g,
                                         juce::ScrollBar& bar,
                                         int x,
                                         int y,
                                         int width,
                                         int height,
                                         bool isScrollbarVertical,
                                         int thumbStartPosition,
                                         int thumbSize,
                                         bool isMouseOver,
                                         bool isMouseDown)
{
    if (thumbSize <= 0)
        return;

    const auto inset = static_cast<float> (appearance.scaled (metrics::scrollbarThumbInset));

    const auto thumb =
        (isScrollbarVertical ? juce::Rectangle<int> { x, thumbStartPosition, width, thumbSize }
                             : juce::Rectangle<int> { thumbStartPosition, y, thumbSize, height })
            .toFloat()
            .reduced (inset);

    const auto hovered = isMouseOver || isMouseDown;

    g.setColour (hovered ? colour (ColourToken::scrollbarHover)
                         : bar.findColour (juce::ScrollBar::thumbColourId));
    g.fillRoundedRectangle (thumb,
                            thumb.getWidth() < thumb.getHeight() ? thumb.getWidth() * 0.5F
                                                                 : thumb.getHeight() * 0.5F);
}

void GraphiteLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                                juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool shouldDrawButtonAsDown)
{
    // Flat: one fill, one hairline, and a small radius. The state is a step of
    // brightness rather than a gradient or a bevel.
    const auto step = [&]
    {
        if (shouldDrawButtonAsDown)
            return 0.12F;

        return shouldDrawButtonAsHighlighted ? 0.06F : 0.0F;
    }();

    const auto fill = backgroundColour.contrasting (step);

    const auto bounds = button.getLocalBounds().toFloat();
    const auto radius = static_cast<float> (appearance.scaled (metrics::radiusMedium));

    g.setColour (button.isEnabled() ? fill : fill.withMultipliedAlpha (0.5F));
    g.fillRoundedRectangle (bounds, radius);

    g.setColour (colour (ColourToken::borderDefault));
    g.drawRoundedRectangle (bounds.reduced (0.5F), radius, 1.0F);
}

juce::Font GraphiteLookAndFeel::getTextButtonFont (juce::TextButton& /*button*/,
                                                   int /*buttonHeight*/)
{
    return interFont (appearance.scaled (typography::body));
}

juce::Font GraphiteLookAndFeel::getLabelFont (juce::Label& /*label*/)
{
    return interFont (appearance.scaled (typography::body));
}

juce::Font GraphiteLookAndFeel::getComboBoxFont (juce::ComboBox& /*box*/)
{
    return interFont (appearance.scaled (typography::body));
}

juce::Font GraphiteLookAndFeel::getPopupMenuFont()
{
    return interFont (appearance.scaled (typography::body));
}
} // namespace duet::gui
