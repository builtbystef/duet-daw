#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace duet::gui
{
/** The app-global settings store, as the interface sees it.

    What belongs here is what the producer sets once and expects on every project:
    theme, interface scale, window geometry, autosave interval, the browser's
    sample folders and favorites. Per-project view state is not this — that lives
    in the VIEW child of the DUET tree, and travels with the project.

    duet_app implements this over the engine's PropertyStorage. The interface is
    string in, string out, so this module needs no engine, no JUCE and no file: a
    view-model over a store held in memory behaves exactly as one over the real
    one, which is how a stored choice is asserted across a restart with nothing
    on disk.
*/
class Settings
{
public:
    virtual ~Settings() = default;

    Settings (const Settings& other) = delete;
    Settings& operator= (const Settings& other) = delete;

    /** What is stored under a key, or nothing when the producer has never set
        it — which is what a first launch looks like.
    */
    [[nodiscard]] virtual std::optional<std::string> value (std::string_view key) const = 0;

    /** Stores a value under a key, replacing whatever was there. */
    virtual void setValue (std::string_view key, std::string_view newValue) = 0;

protected:
    Settings() = default;
};
} // namespace duet::gui
