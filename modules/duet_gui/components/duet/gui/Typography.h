#pragma once

#include <juce_graphics/juce_graphics.h>

/** Inter, the application typeface.

    Compiled into the binary rather than looked for on the machine, so the
    Graphite look reads the same on a machine that has never heard of Inter. The
    licence (SIL OFL 1.1) travels with the font files, beside them in the module.
*/
namespace duet::gui
{
/** Inter Regular, loaded once for the run of the app. */
[[nodiscard]] juce::Typeface::Ptr interRegular();

/** Inter Bold, loaded once for the run of the app. */
[[nodiscard]] juce::Typeface::Ptr interBold();

/** The chrome's font, at a height in pixels. */
[[nodiscard]] juce::Font interFont (float heightPx, bool bold = false);

/** The same font asking Inter for tabular figures, for anything that shows a
    number that changes.

    Inter's proportional digits are not all the same width, so a readout counting
    up in them shifts sideways as it counts — the bar/beat display, the wall
    clock, a dB value, a BPM. With `tnum` on, every digit takes one advance and
    the readout stands still.
*/
[[nodiscard]] juce::Font readoutFont (float heightPx, bool bold = false);
} // namespace duet::gui
