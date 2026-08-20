#pragma once

#include <string_view>

namespace duet::gui
{
class Settings;

namespace settingKey
{
    inline constexpr std::string_view hardwareAcceleration = "interface.hardwareAcceleration";
} // namespace settingKey

/** Whether the producer has asked for the rendering escape hatch.

    Off unless they have: every surface is drawn on JUCE's software renderer,
    which spec 535bbo settled after the prototype held 60 tracks on it. The
    hatch is for a machine where one ever stops holding, and what it changes is
    who rasterises the drawing — never the drawing.
*/
[[nodiscard]] bool hardwareAccelerationEnabled (const Settings& store);

/** Stores the producer's choice, for this launch and the next. */
void setHardwareAccelerationEnabled (Settings& store, bool shouldBeEnabled);
} // namespace duet::gui
