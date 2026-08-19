#include <duet/gui/Typography.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

using Catch::Approx;

namespace
{
float widthOf (const juce::Font& font, const juce::String& text)
{
    return juce::GlyphArrangement::getStringWidth (font, text);
}
} // namespace

TEST_CASE ("Inter is inside the binary")
{
    const juce::ScopedJuceInitialiser_GUI juce;

    REQUIRE (duet::gui::interRegular() != nullptr);
    REQUIRE (duet::gui::interBold() != nullptr);
    INFO ("regular is " << duet::gui::interRegular()->getName());
    INFO ("bold is " << duet::gui::interBold()->getName());
    REQUIRE (duet::gui::interRegular()->getName().containsIgnoreCase ("Inter"));
}

TEST_CASE ("a readout's digits all take the same width")
{
    const juce::ScopedJuceInitialiser_GUI juce;

    const auto readout = duet::gui::readoutFont (13.0F);
    const auto advance = widthOf (readout, "0");

    for (auto digit = '1'; digit <= '9'; ++digit)
    {
        INFO ("digit " << digit);
        REQUIRE (widthOf (readout, juce::String::charToString (digit)) == Approx (advance));
    }

    // What the criterion is about: a bar/beat readout counting up stands still.
    REQUIRE (widthOf (readout, "001.1.000") == Approx (widthOf (readout, "128.4.479")));

    // And that it is the tabular figures doing it, not a coincidence of the
    // typeface: Inter's proportional 1 is a good deal narrower than its 0.
    const auto chrome = duet::gui::interFont (13.0F);

    REQUIRE (widthOf (chrome, "1") < widthOf (chrome, "0"));
}
