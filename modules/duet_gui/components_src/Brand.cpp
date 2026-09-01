#include <duet/gui/Brand.h>

#include <DuetBrand.h>

namespace duet::gui
{
namespace
{
    std::unique_ptr<juce::Drawable> load (const char* data, int size)
    {
        return juce::Drawable::createFromImageData (data, static_cast<std::size_t> (size));
    }
} // namespace

std::unique_ptr<juce::Drawable> brandMark()
{
    return load (DuetBrand::duetmark_svg, DuetBrand::duetmark_svgSize);
}

std::unique_ptr<juce::Drawable> brandMark (juce::Colour ink)
{
    auto mark = brandMark();

    if (mark != nullptr)
        mark->replaceColour (juce::Colour { 0xff1c1f26 }, ink);

    return mark;
}

std::unique_ptr<juce::Drawable> brandWordmark()
{
    return load (DuetBrand::duetwordmark_svg, DuetBrand::duetwordmark_svgSize);
}

juce::Image brandMarkImage (int sizePx)
{
    juce::Image image { juce::Image::ARGB, sizePx, sizePx, true };
    juce::Graphics g { image };

    if (const auto mark = brandMark(); mark != nullptr)
        mark->drawWithin (g,
                          { 0.0F, 0.0F, static_cast<float> (sizePx), static_cast<float> (sizePx) },
                          juce::RectanglePlacement::centred,
                          1.0F);

    return image;
}
} // namespace duet::gui
