#include <duet/gui/AutosaveSettings.h>

#include <duet/gui/Settings.h>

namespace duet::gui
{
namespace
{
    constexpr std::string_view autosaveIntervalKey = "autosaveIntervalMinutes";

    std::string_view storedValue (duet::persistence::AutosaveInterval interval)
    {
        using enum duet::persistence::AutosaveInterval;

        switch (interval)
        {
            case off:
                return "off";
            case twoMinutes:
                return "2";
            case fiveMinutes:
                return "5";
            case tenMinutes:
                return "10";
        }

        return "10";
    }
} // namespace

//==============================================================================
duet::persistence::AutosaveInterval autosaveInterval (const Settings& settings)
{
    using enum duet::persistence::AutosaveInterval;

    const auto stored = settings.value (autosaveIntervalKey);

    if (! stored.has_value())
        return duet::persistence::defaultAutosaveInterval;
    if (*stored == "off")
        return off;
    if (*stored == "2")
        return twoMinutes;
    if (*stored == "5")
        return fiveMinutes;
    if (*stored == "10")
        return tenMinutes;

    return duet::persistence::defaultAutosaveInterval;
}

void setAutosaveInterval (Settings& settings, duet::persistence::AutosaveInterval interval)
{
    settings.setValue (autosaveIntervalKey, storedValue (interval));
}
} // namespace duet::gui
