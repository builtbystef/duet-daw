#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

/** The Graphite visual language, as values.

    One token set carries both palettes, so a component asks for a token and a
    theme and never writes a colour of its own. The values come from
    `docs/ui-tokens.css` — the vendored copy of the visual reference's token
    stylesheet — with the r4m858 amendments its header lists applied.

    Nothing here paints. This header is part of the paintless half of duet_gui
    and names no JUCE type, so the token set can be asserted with no window on
    screen.
*/
namespace duet::gui
{
/** Which of the two palettes is in force. */
enum class Theme : std::uint8_t
{
    dark,
    light
};

/** A colour token's value: 8-bit sRGB channels, and the alpha the translucent
    chrome tokens carry.
*/
struct Colour
{
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    std::uint8_t alpha = 255;

    friend constexpr bool operator== (const Colour& first, const Colour& second) = default;
};

/** A colour written the way the visual reference writes it: 0xrrggbb. */
[[nodiscard]] constexpr Colour rgb (std::uint32_t hex)
{
    return { static_cast<std::uint8_t> ((hex >> 16) & 0xffU),
             static_cast<std::uint8_t> ((hex >> 8) & 0xffU),
             static_cast<std::uint8_t> (hex & 0xffU),
             0xff };
}

/** The same, with the fractional alpha the reference's `rgba()` tokens carry. */
[[nodiscard]] constexpr Colour rgba (std::uint32_t hex, double alpha)
{
    auto colour = rgb (hex);

    // Rounded to nearest without the (x + 0.5) idiom, which is wrong for a
    // negative and which the linter rejects for that reason. An alpha is never
    // negative.
    const auto scaled = alpha * 255.0;
    const auto whole = static_cast<std::uint32_t> (scaled);

    colour.alpha = static_cast<std::uint8_t> (scaled - whole < 0.5 ? whole : whole + 1);
    return colour;
}

/** Every colour the Graphite look has a name for.

    The groups are the reference stylesheet's: surfaces, rules, text, accent, the
    Collaborator's one reserved hue, the semantic four, the eight assignable
    track colours, and the chrome the timeline and the piano roll are drawn with.
*/
enum class ColourToken : std::uint8_t
{
    surfaceCanvas,
    surfaceDefault,
    surfaceRaised,
    surfaceInteractive,

    borderDefault,
    borderSubtle,

    textPrimary,
    textSecondary,
    textMuted,
    textDisabled,

    accentStrong,
    accentDefault,
    accentBright,
    onAccent,

    /** The Collaborator's hue, and the only hue in the whole set that means one
        thing: ✦ badges, Suggestion ghosts and their glow, ghost fader handles,
        commentary accents. Never used for anything else.
    */
    collaborator,

    semanticInfo,
    semanticSuccess,
    semanticWarning,
    semanticDanger,

    trackOrange,
    trackCoral,
    trackMint,
    trackCyan,
    trackYellow,
    trackRed,
    trackPurple,
    trackBlue,
    onTrack,

    gridFine,
    gridBeat,
    gridBar,
    meterTrack,
    keyBlack,
    keyWhite,
    overlayStrong,
    overlaySoft,
    overlayScrim,
    shadow,
    scrollbarHover
};

/** What a token is worth under a palette. */
[[nodiscard]] Colour colour (ColourToken token, Theme theme);

/** Every token, in declaration order. */
[[nodiscard]] std::span<const ColourToken> allColourTokens();

/** How many colours a producer can give a track. */
inline constexpr std::size_t trackColourCount = 8;

/** The track colour at a position in the assignable set.

    Positions past the end wrap, so numbering tracks from zero gives each one a
    colour without the caller counting.
*/
[[nodiscard]] ColourToken trackColourToken (std::size_t index);

/** The measurements the Graphite look is drawn to, in logical units: the
    interface scale is what turns one into pixels.
*/
namespace metrics
{
    inline constexpr int radiusSmall = 3;
    inline constexpr int radiusMedium = 4;
    inline constexpr int radiusLarge = 6;
    inline constexpr int radiusExtraLarge = 8;

    /** The thin workstation scrollbar: a track the width of a hairline gap and
        a thumb inset into it.
    */
    inline constexpr int scrollbarThickness = 9;
    inline constexpr int scrollbarThumbInset = 2;

    /** Mid-density chrome: the height a row of controls occupies, and the gap
        between two of them.
    */
    inline constexpr int rowHeight = 24;
    inline constexpr int rowGap = 6;
    inline constexpr int panelPadding = 10;
} // namespace metrics

/** The type, in logical units. Inter throughout — the chrome reads at 11 and the
    section eyebrows at 9, which is what the visual reference sets them at.
*/
namespace typography
{
    inline constexpr std::string_view family = "Inter";
    inline constexpr float body = 11.0F;
    inline constexpr float eyebrow = 9.0F;
    inline constexpr float eyebrowLetterSpacing = 0.08F;
} // namespace typography
} // namespace duet::gui
