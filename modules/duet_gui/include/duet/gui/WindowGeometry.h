#pragma once

#include <optional>
#include <string_view>

namespace duet::gui
{
class Settings;

namespace settingKey
{
    inline constexpr std::string_view windowBounds = "interface.windowBounds";
} // namespace settingKey

/** Where the main window is and how big it is, in the desktop's own pixels. */
struct WindowBounds
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    friend constexpr bool operator== (const WindowBounds& first,
                                      const WindowBounds& second) = default;
};

/** The size the main window opens at when the producer has never moved one.
    Wide enough for the arrangement between two open docks.
*/
inline constexpr int defaultWindowWidth = 1600;
inline constexpr int defaultWindowHeight = 980;

/** Where the producer left the main window, or nothing on a first launch.

    Window geometry is app-global and not part of any project: the window comes
    back where it was whichever project opens in it (spec 535bbo). Nothing, too,
    when what is stored is not a geometry a window could be opened at — a
    settings file is a text file, and one that has been edited by hand still has
    to launch.
*/
[[nodiscard]] std::optional<WindowBounds> storedWindowBounds (const Settings& store);

/** Stores where the main window is now. */
void storeWindowBounds (Settings& store, WindowBounds bounds);
} // namespace duet::gui
