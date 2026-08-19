#include <duet/gui/Typography.h>

#include <DuetFonts.h>

namespace duet::gui
{
namespace
{
    /** The tag OpenType gives tabular figures. */
    constexpr juce::FontFeatureTag tabularFigures { "tnum" };

    juce::Typeface::Ptr load (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, static_cast<std::size_t> (size));
    }
} // namespace

juce::Typeface::Ptr interRegular()
{
    static const juce::Typeface::Ptr typeface =
        load (DuetFonts::InterRegular_ttf, DuetFonts::InterRegular_ttfSize);

    return typeface;
}

juce::Typeface::Ptr interBold()
{
    static const juce::Typeface::Ptr typeface =
        load (DuetFonts::InterBold_ttf, DuetFonts::InterBold_ttfSize);

    return typeface;
}

juce::Font interFont (float heightPx, bool bold)
{
    return juce::Font { juce::FontOptions { bold ? interBold() : interRegular() }.withHeight (
        heightPx) };
}

juce::Font readoutFont (float heightPx, bool bold)
{
    return juce::Font { juce::FontOptions { bold ? interBold() : interRegular() }
                            .withHeight (heightPx)
                            .withFeatureEnabled (tabularFigures) };
}
} // namespace duet::gui
