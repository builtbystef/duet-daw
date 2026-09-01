#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

/** The Duet mark, compiled into the binary the way the typeface is.

    The mark is the charcoal D beside the Collaborator-teal play triangle, and
    the wordmark is the same pair spelling the product's name. They appear on
    branding surfaces only — the window's icon, the button that opens the Duet
    menu, and the About window — so the interface itself stays within the
    Graphite palette and the teal keeps its one reserved meaning.
*/
namespace duet::gui
{
/** The square mark, as a drawable in its own brand colours. */
[[nodiscard]] std::unique_ptr<juce::Drawable> brandMark();

/** The square mark re-inked for the surface it sits on: the charcoal D takes
    the ink the surface's text does, and the teal triangle keeps its brand
    colour on both themes.
*/
[[nodiscard]] std::unique_ptr<juce::Drawable> brandMark (juce::Colour ink);

/** The full wordmark — the mark's pair spelling "Duet DAW". */
[[nodiscard]] std::unique_ptr<juce::Drawable> brandWordmark();

/** The square mark rendered at a size in pixels, for a window icon. */
[[nodiscard]] juce::Image brandMarkImage (int sizePx);
} // namespace duet::gui
