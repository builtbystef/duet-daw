#include <duet/gui/WindowGeometry.h>

#include <duet/gui/Settings.h>

#include <array>
#include <charconv>
#include <string>
#include <system_error>

namespace duet::gui
{
namespace
{
    /** The four numbers, in the order the geometry is written in. */
    constexpr char separator = ' ';

    /** Reads one number and leaves the reader on what follows it. Nothing when
        what is there is not a number.
    */
    std::optional<int> readNumber (std::string_view& remaining)
    {
        int value = 0;
        const auto read =
            std::from_chars (remaining.data(), remaining.data() + remaining.size(), value);

        if (read.ec != std::errc {})
            return std::nullopt;

        remaining.remove_prefix (static_cast<std::size_t> (read.ptr - remaining.data()));

        if (! remaining.empty() && remaining.front() == separator)
            remaining.remove_prefix (1);

        return value;
    }
} // namespace

std::optional<WindowBounds> storedWindowBounds (const Settings& store)
{
    const auto text = store.value (settingKey::windowBounds);

    if (! text.has_value())
        return std::nullopt;

    std::string_view remaining { *text };
    WindowBounds bounds;

    for (auto* number : { &bounds.x, &bounds.y, &bounds.width, &bounds.height })
    {
        const auto read = readNumber (remaining);

        if (! read.has_value())
            return std::nullopt;

        *number = *read;
    }

    // Anything left over was not written by the line below, and a geometry with
    // no extent is not one a window can be opened at.
    if (! remaining.empty() || bounds.width <= 0 || bounds.height <= 0)
        return std::nullopt;

    return bounds;
}

void storeWindowBounds (Settings& store, WindowBounds bounds)
{
    const auto number = [] (int value) { return std::to_string (value); };

    store.setValue (settingKey::windowBounds,
                    number (bounds.x) + separator + number (bounds.y) + separator
                        + number (bounds.width) + separator + number (bounds.height));
}
} // namespace duet::gui
