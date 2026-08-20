#pragma once

#include <duet/gui/Settings.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <map>
#include <optional>
#include <string>
#include <string_view>

/** What this suite needs and the paintless one gets from duet::test_support.

    The interface's JUCE-linked suite deliberately links duet::gui_components and
    nothing else — no engine, no facades — so it cannot reach that library, and
    the little it shares lives here instead.
*/
namespace duet::testing
{
/** The app-global store, held in memory. */
class StoredSettings final : public duet::gui::Settings
{
public:
    [[nodiscard]] std::optional<std::string> value (std::string_view key) const override
    {
        const auto found = values.find (std::string { key });

        return found == values.end() ? std::nullopt : std::optional { found->second };
    }

    void setValue (std::string_view key, std::string_view newValue) override
    {
        values[std::string { key }] = std::string { newValue };
    }

private:
    std::map<std::string, std::string> values;
};

/** A component of a surface's name, wherever it is in the tree under this one. */
[[nodiscard]] inline const juce::Component* surfaceOf (const juce::Component& root,
                                                       const char* surfaceId)
{
    if (root.getComponentID() == surfaceId)
        return &root;

    for (const auto* child : root.getChildren())
        if (const auto* found = surfaceOf (*child, surfaceId); found != nullptr)
            return found;

    return nullptr;
}
} // namespace duet::testing
