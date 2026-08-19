#include <duet/gui/Appearance.h>
#include <duet/gui/GraphiteLookAndFeel.h>
#include <duet/gui/Settings.h>
#include <duet/gui/Tokens.h>

#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include <map>
#include <optional>
#include <string>
#include <string_view>

using duet::gui::Appearance;
using duet::gui::ColourToken;
using duet::gui::GraphiteLookAndFeel;
using duet::gui::Theme;
using duet::gui::ThemePreference;
using duet::gui::toJuce;

namespace
{
/** The app-global store, held in memory. */
class StoredSettings final : public duet::gui::Settings
{
public:
    [[nodiscard]] std::optional<std::string> value (std::string_view key) const override
    {
        const auto found = values.find (std::string { key });

        return found == values.end() ? std::nullopt : std::optional { found->second };
    }

    void setValue (std::string_view key, std::string_view newValue) override
    {
        values[std::string { key }] = std::string { newValue };
    }

private:
    std::map<std::string, std::string> values;
};
} // namespace

TEST_CASE ("the look paints from the token set, and follows the theme")
{
    const juce::ScopedJuceInitialiser_GUI juce;

    StoredSettings store;
    Appearance appearance { store, true };
    const GraphiteLookAndFeel look { appearance };

    REQUIRE (look.findColour (juce::ResizableWindow::backgroundColourId)
             == toJuce (colour (ColourToken::surfaceCanvas, Theme::dark)));
    REQUIRE (look.findColour (juce::Label::textColourId)
             == toJuce (colour (ColourToken::textPrimary, Theme::dark)));

    appearance.setThemePreference (ThemePreference::light);

    REQUIRE (look.findColour (juce::ResizableWindow::backgroundColourId)
             == toJuce (colour (ColourToken::surfaceCanvas, Theme::light)));
    REQUIRE (look.findColour (juce::Label::textColourId)
             == toJuce (colour (ColourToken::textPrimary, Theme::light)));
}

TEST_CASE ("scrollbars are the thin workstation form, and scale with the interface")
{
    const juce::ScopedJuceInitialiser_GUI juce;

    StoredSettings store;
    Appearance appearance { store, true };
    GraphiteLookAndFeel look { appearance };
    juce::ScrollBar bar { true };

    appearance.setInterfaceScale (1.0);

    REQUIRE (look.getDefaultScrollbarWidth() == duet::gui::metrics::scrollbarThickness);
    REQUIRE (look.getScrollbarButtonSize (bar) == 0);

    appearance.setInterfaceScale (2.0);

    REQUIRE (look.getDefaultScrollbarWidth() == 2 * duet::gui::metrics::scrollbarThickness);
}
