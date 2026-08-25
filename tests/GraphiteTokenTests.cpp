#include <duet/gui/Tokens.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

using duet::gui::allColourTokens;
using duet::gui::Colour;
using duet::gui::colour;
using duet::gui::ColourToken;
using duet::gui::rgb;
using duet::gui::rgba;
using duet::gui::Theme;
using duet::gui::trackColourCount;
using duet::gui::trackColourToken;

namespace
{
/** The reference's values, read out of docs/ui-tokens.css: the dark block with
    the r4m858 text amendments its header lists, and the light block verbatim.
*/
struct TokenValue
{
    ColourToken token = ColourToken::surfaceCanvas;
    Colour dark;
    Colour light;
};

constexpr std::array graphite {
    TokenValue { ColourToken::surfaceCanvas, rgb (0x0a0a0a), rgb (0xeeeeee) },
    TokenValue { ColourToken::surfaceDefault, rgb (0x101010), rgb (0xf8f8f8) },
    TokenValue { ColourToken::surfaceRaised, rgb (0x181818), rgb (0xffffff) },
    TokenValue { ColourToken::surfaceInteractive, rgb (0x212121), rgb (0xe3e3e3) },
    TokenValue { ColourToken::borderDefault, rgb (0x282828), rgb (0xd5d5d5) },
    TokenValue { ColourToken::borderSubtle, rgb (0x1c1c1c), rgb (0xe8e8e8) },

    TokenValue { ColourToken::textPrimary, rgb (0xd2d2d2), rgb (0x1a1a1a) },
    TokenValue { ColourToken::textSecondary, rgb (0x9e9e9e), rgb (0x4a4a4a) },
    TokenValue { ColourToken::textMuted, rgb (0x828282), rgb (0x6b6b6b) },
    TokenValue { ColourToken::textDisabled, rgb (0x5c5c5c), rgb (0x9a9a9a) },

    TokenValue { ColourToken::accentStrong, rgb (0xc9c9c9), rgb (0x1f1f1f) },
    TokenValue { ColourToken::accentDefault, rgb (0xdedede), rgb (0x2e2e2e) },
    TokenValue { ColourToken::accentBright, rgb (0xf4f4f4), rgb (0x454545) },
    TokenValue { ColourToken::onAccent, rgb (0x0a0a0a), rgb (0xfafafa) },

    TokenValue { ColourToken::collaborator, rgb (0x4aa294), rgb (0x0e7c70) },

    TokenValue { ColourToken::semanticInfo, rgb (0x7ba7cc), rgb (0x2c6f9e) },
    TokenValue { ColourToken::semanticSuccess, rgb (0x74b57f), rgb (0x2d8340) },
    TokenValue { ColourToken::semanticWarning, rgb (0xcfa055), rgb (0x96691a) },
    TokenValue { ColourToken::semanticDanger, rgb (0xcc6560), rgb (0xac3e39) },

    TokenValue { ColourToken::trackOrange, rgb (0xab7c4c), rgb (0x946420) },
    TokenValue { ColourToken::trackCoral, rgb (0xac6558), rgb (0x9c4c3f) },
    TokenValue { ColourToken::trackMint, rgb (0x5f9b81), rgb (0x35785f) },
    TokenValue { ColourToken::trackCyan, rgb (0x52909e), rgb (0x2f6f7d) },
    TokenValue { ColourToken::trackYellow, rgb (0xa09354), rgb (0x7d6a26) },
    TokenValue { ColourToken::trackRed, rgb (0xa45555), rgb (0x94403f) },
    TokenValue { ColourToken::trackPurple, rgb (0x806fa3), rgb (0x635383) },
    TokenValue { ColourToken::trackBlue, rgb (0x5b7ba5), rgb (0x43618a) },
    TokenValue { ColourToken::onTrack, rgb (0x0a0a0a), rgb (0xfbfbfb) },

    TokenValue { ColourToken::gridFine, rgba (0xffffff, 0.03), rgba (0x000000, 0.045) },
    TokenValue { ColourToken::gridBeat, rgba (0xffffff, 0.055), rgba (0x000000, 0.075) },
    TokenValue { ColourToken::gridBar, rgba (0xffffff, 0.12), rgba (0x000000, 0.15) },
    TokenValue { ColourToken::meterTrack, rgba (0xffffff, 0.05), rgba (0x000000, 0.08) },
    TokenValue { ColourToken::keyBlack, rgb (0x0e0e0e), rgb (0xe0e0e0) },
    TokenValue { ColourToken::keyWhite, rgb (0x1d1d1d), rgb (0xfcfcfc) },
    TokenValue { ColourToken::overlayStrong, rgba (0x000000, 0.42), rgba (0x000000, 0.09) },
    TokenValue { ColourToken::overlaySoft, rgba (0x000000, 0.22), rgba (0x000000, 0.045) },
    TokenValue { ColourToken::overlayScrim, rgba (0x000000, 0.3), rgba (0x000000, 0.06) },
    TokenValue { ColourToken::shadow, rgba (0x000000, 0.68), rgba (0x000000, 0.16) },
    TokenValue { ColourToken::scrollbarHover, rgb (0x313131), rgb (0xc4c4c4) },
};

/** The hue a colour reads as, in degrees, and how far from grey it is. A pair of
    achromatic tokens has no hue to compare, so the saturation comes with it.
*/
std::pair<double, double> hueAndSaturation (Colour value)
{
    const auto red = value.red / 255.0;
    const auto green = value.green / 255.0;
    const auto blue = value.blue / 255.0;

    const auto high = std::max ({ red, green, blue });
    const auto low = std::min ({ red, green, blue });
    const auto span = high - low;

    if (span <= 0.0)
        return { 0.0, 0.0 };

    const auto hue = [&]
    {
        if (high == red)
            return 60.0 * (((green - blue) / span) + (green < blue ? 6.0 : 0.0));

        if (high == green)
            return 60.0 * (((blue - red) / span) + 2.0);

        return 60.0 * (((red - green) / span) + 4.0);
    }();

    return { hue, span / high };
}

double hueDistance (Colour a, Colour b)
{
    const auto difference = std::abs (hueAndSaturation (a).first - hueAndSaturation (b).first);
    return std::min (difference, 360.0 - difference);
}
} // namespace

TEST_CASE ("one token set carries both palettes")
{
    for (const auto& [token, dark, light] : graphite)
    {
        INFO ("token " << static_cast<int> (token));
        REQUIRE (colour (token, Theme::dark) == dark);
        REQUIRE (colour (token, Theme::light) == light);
    }
}

TEST_CASE ("the token set has a value for every token it names")
{
    REQUIRE (allColourTokens().size() == graphite.size());

    for (std::size_t index = 0; index < graphite.size(); ++index)
        REQUIRE (allColourTokens()[index] == graphite.at (index).token);
}

TEST_CASE ("the Collaborator's teal is one token pair and appears nowhere else")
{
    REQUIRE (colour (ColourToken::collaborator, Theme::dark) == rgb (0x4aa294));
    REQUIRE (colour (ColourToken::collaborator, Theme::light) == rgb (0x0e7c70));

    for (const auto theme : { Theme::dark, Theme::light })
    {
        const auto teal = colour (ColourToken::collaborator, theme);

        for (const auto token : allColourTokens())
        {
            if (token == ColourToken::collaborator)
                continue;

            INFO ("token " << static_cast<int> (token));
            REQUIRE_FALSE (colour (token, theme) == teal);
        }
    }
}

TEST_CASE ("the semantic four keep their own hues, none of them the Collaborator's")
{
    constexpr std::array semantics { ColourToken::semanticInfo,
                                     ColourToken::semanticSuccess,
                                     ColourToken::semanticWarning,
                                     ColourToken::semanticDanger };

    for (const auto theme : { Theme::dark, Theme::light })
    {
        const auto teal = colour (ColourToken::collaborator, theme);

        for (std::size_t first = 0; first < semantics.size(); ++first)
        {
            const auto value = colour (semantics.at (first), theme);

            INFO ("token " << static_cast<int> (semantics.at (first)));
            REQUIRE (hueDistance (value, teal) > 30.0);

            for (auto second = first + 1; second < semantics.size(); ++second)
                REQUIRE (hueDistance (value, colour (semantics.at (second), theme)) > 30.0);
        }
    }
}

TEST_CASE ("eight track colours are assignable in both modes")
{
    REQUIRE (trackColourCount == 8);

    for (const auto theme : { Theme::dark, Theme::light })
    {
        std::vector<Colour> assigned;

        for (std::size_t index = 0; index < trackColourCount; ++index)
            assigned.push_back (colour (trackColourToken (index), theme));

        for (std::size_t first = 0; first < assigned.size(); ++first)
        {
            INFO ("track colour " << first);
            REQUIRE (hueAndSaturation (assigned.at (first)).second < 0.8);
            REQUIRE_FALSE (assigned.at (first) == colour (ColourToken::collaborator, theme));

            for (auto second = first + 1; second < assigned.size(); ++second)
                REQUIRE_FALSE (assigned.at (first) == assigned.at (second));
        }
    }
}

TEST_CASE ("track colours past the eighth wrap round the set")
{
    REQUIRE (trackColourToken (0) == ColourToken::trackOrange);
    REQUIRE (trackColourToken (trackColourCount) == trackColourToken (0));
    REQUIRE (trackColourToken (trackColourCount + 3) == trackColourToken (3));
}

TEST_CASE ("the Collaborator's hue and badge are reserved to the Collaborator's own surfaces")
{
    // The teal token and the four-pointed badge mean one thing in this
    // interface, and a rule about the whole tree is what keeps them meaning it.
    // Its own header declares the reservation; the Collaborator panel and the
    // Suggestion surfaces are what may draw it.
    const auto isAllowed = [] (const std::filesystem::path& file)
    {
        const auto name = file.filename().string();

        return name.starts_with ("Tokens.") || name.find ("Collaborator") != std::string::npos
               || name.find ("Suggestion") != std::string::npos;
    };

    std::vector<std::string> offenders;

    for (const auto& entry : std::filesystem::recursive_directory_iterator { DUET_MODULES_DIR })
    {
        const auto& file = entry.path();

        if (! entry.is_regular_file() || (file.extension() != ".h" && file.extension() != ".cpp"))
            continue;

        std::ifstream source { file };
        const std::string text { std::istreambuf_iterator<char> { source },
                                 std::istreambuf_iterator<char> {} };

        const auto usesTheHue = text.find ("ColourToken::collaborator") != std::string::npos;
        const auto usesTheBadge = text.find ("✦") != std::string::npos
                                  || text.find ("CollaboratorBadge") != std::string::npos;

        if ((usesTheHue || usesTheBadge) && ! isAllowed (file))
            offenders.push_back (file.filename().string());
    }

    REQUIRE (offenders.empty());
}
