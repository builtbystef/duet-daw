#include <duet/gui/Tokens.h>

#include <array>

namespace duet::gui
{
namespace
{
    /** One token's two values. The order of the table is the order of the enum,
        and `graphite` is indexed by the enumerator, so a token added in one and
        forgotten in the other is a compile error rather than a wrong colour.
    */
    struct Pair
    {
        Colour dark;
        Colour light;
    };

    constexpr std::size_t tokenCount = static_cast<std::size_t> (ColourToken::scrollbarHover) + 1;

    /** The Graphite palette, from `docs/ui-tokens.css` — the vendored copy of the
        visual reference's token stylesheet — with the r4m858 amendments its
        header lists applied: the dark text steps are the brightened ones, and
        the Collaborator's teal, which predates that sheet, is here.
    */
    constexpr std::array<Pair, tokenCount> graphite {
        { { rgb (0x0a0a0a), rgb (0xeeeeee) }, // surfaceCanvas
          { rgb (0x101010), rgb (0xf8f8f8) }, // surfaceDefault
          { rgb (0x181818), rgb (0xffffff) }, // surfaceRaised
          { rgb (0x212121), rgb (0xe3e3e3) }, // surfaceInteractive

          { rgb (0x282828), rgb (0xd5d5d5) }, // borderDefault
          { rgb (0x1c1c1c), rgb (0xe8e8e8) }, // borderSubtle

          { rgb (0xd2d2d2), rgb (0x1a1a1a) }, // textPrimary
          { rgb (0x9e9e9e), rgb (0x4a4a4a) }, // textSecondary
          { rgb (0x828282), rgb (0x6b6b6b) }, // textMuted
          { rgb (0x5c5c5c), rgb (0x9a9a9a) }, // textDisabled

          { rgb (0xc9c9c9), rgb (0x1f1f1f) }, // accentStrong
          { rgb (0xdedede), rgb (0x2e2e2e) }, // accentDefault
          { rgb (0xf4f4f4), rgb (0x454545) }, // accentBright
          { rgb (0x0a0a0a), rgb (0xfafafa) }, // onAccent

          { rgb (0x4aa294), rgb (0x0e7c70) }, // collaborator

          { rgb (0x7ba7cc), rgb (0x2c6f9e) }, // semanticInfo
          { rgb (0x74b57f), rgb (0x2d8340) }, // semanticSuccess
          { rgb (0xcfa055), rgb (0x96691a) }, // semanticWarning
          { rgb (0xcc6560), rgb (0xac3e39) }, // semanticDanger

          { rgb (0xab7c4c), rgb (0x946420) }, // trackOrange
          { rgb (0xac6558), rgb (0x9c4c3f) }, // trackCoral
          { rgb (0x5f9b81), rgb (0x35785f) }, // trackMint
          { rgb (0x52909e), rgb (0x2f6f7d) }, // trackCyan
          { rgb (0xa09354), rgb (0x7d6a26) }, // trackYellow
          { rgb (0xa45555), rgb (0x94403f) }, // trackRed
          { rgb (0x806fa3), rgb (0x635383) }, // trackPurple
          { rgb (0x5b7ba5), rgb (0x43618a) }, // trackBlue
          { rgb (0x0a0a0a), rgb (0xfbfbfb) }, // onTrack

          { rgba (0xffffff, 0.03), rgba (0x000000, 0.045) },  // gridFine
          { rgba (0xffffff, 0.055), rgba (0x000000, 0.075) }, // gridBeat
          { rgba (0xffffff, 0.12), rgba (0x000000, 0.15) },   // gridBar
          { rgba (0xffffff, 0.05), rgba (0x000000, 0.08) },   // meterTrack
          { rgb (0x0e0e0e), rgb (0xe0e0e0) },                 // keyBlack
          { rgb (0x1d1d1d), rgb (0xfcfcfc) },                 // keyWhite
          { rgba (0x000000, 0.42), rgba (0x000000, 0.09) },   // overlayStrong
          { rgba (0x000000, 0.22), rgba (0x000000, 0.045) },  // overlaySoft
          { rgba (0x000000, 0.3), rgba (0x000000, 0.06) },    // overlayScrim
          { rgba (0x000000, 0.68), rgba (0x000000, 0.16) },   // shadow
          { rgb (0x313131), rgb (0xc4c4c4) } }                // scrollbarHover
    };

    /** Every token, in declaration order, built once from the enumerators. */
    constexpr std::array<ColourToken, tokenCount> everyToken = []
    {
        std::array<ColourToken, tokenCount> tokens {};

        for (std::size_t index = 0; index < tokenCount; ++index)
            tokens.at (index) = static_cast<ColourToken> (index);

        return tokens;
    }();

    /** The assignable set, in the order a producer meets it. */
    constexpr std::array<ColourToken, trackColourCount> trackColours {
        ColourToken::trackOrange, ColourToken::trackCoral,  ColourToken::trackMint,
        ColourToken::trackCyan,   ColourToken::trackYellow, ColourToken::trackRed,
        ColourToken::trackPurple, ColourToken::trackBlue
    };
} // namespace

Colour colour (ColourToken token, Theme theme)
{
    const auto& pair = graphite.at (static_cast<std::size_t> (token));

    return theme == Theme::dark ? pair.dark : pair.light;
}

std::span<const ColourToken> allColourTokens() { return everyToken; }

ColourToken trackColourToken (std::size_t index)
{
    return trackColours.at (index % trackColourCount);
}
} // namespace duet::gui
