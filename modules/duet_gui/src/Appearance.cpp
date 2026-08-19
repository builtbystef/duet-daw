#include <duet/gui/Appearance.h>

#include <duet/gui/Settings.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace duet::gui
{
namespace
{
    /** The stored spelling of each preference. Words and not numbers: the store
        is a file a producer can open, and a renumbered enum must not silently
        change what an old file means.
    */
    constexpr std::array<std::pair<ThemePreference, std::string_view>, 3> themeNames {
        { { ThemePreference::followSystem, "followSystem" },
          { ThemePreference::dark, "dark" },
          { ThemePreference::light, "light" } }
    };

    ThemePreference storedPreference (const Settings& settings)
    {
        const auto stored = settings.value (settingKey::theme);

        if (! stored.has_value())
            return ThemePreference::followSystem;

        for (const auto& [preference, name] : themeNames)
            if (*stored == name)
                return preference;

        // A store written by a version that knew a preference this one does not.
        // The desktop is the answer that is always right.
        return ThemePreference::followSystem;
    }

    double storedScale (const Settings& settings)
    {
        const auto stored = settings.value (settingKey::interfaceScale);

        if (! stored.has_value())
            return defaultInterfaceScale;

        try
        {
            return std::clamp (std::stod (*stored),
                               Appearance::minimumInterfaceScale,
                               Appearance::maximumInterfaceScale);
        }
        catch (const std::exception&)
        {
            return defaultInterfaceScale;
        }
    }
} // namespace

Theme resolveTheme (ThemePreference preference, bool systemIsDark)
{
    switch (preference)
    {
        case ThemePreference::dark:
            return Theme::dark;

        case ThemePreference::light:
            return Theme::light;

        case ThemePreference::followSystem:
        default:
            return systemIsDark ? Theme::dark : Theme::light;
    }
}

Appearance::Appearance (Settings& store, bool systemIsDark)
    : settings (store), desktopIsDark (systemIsDark), preference (storedPreference (store)),
      scale (storedScale (store)), resolved (resolveTheme (preference, systemIsDark))
{
}

Appearance::~Appearance() = default;

void Appearance::addListener (Listener* listener) { listeners.push_back (listener); }

void Appearance::removeListener (Listener* listener)
{
    listeners.erase (std::remove (listeners.begin(), listeners.end(), listener), listeners.end());
}

void Appearance::setThemePreference (ThemePreference newPreference)
{
    if (newPreference == preference)
        return;

    preference = newPreference;

    for (const auto& [candidate, name] : themeNames)
        if (candidate == preference)
            settings.setValue (settingKey::theme, name);

    resolve();
}

void Appearance::systemDarkModeChanged (bool systemIsDark)
{
    if (systemIsDark == desktopIsDark)
        return;

    desktopIsDark = systemIsDark;
    resolve();
}

void Appearance::setInterfaceScale (double newScale)
{
    const auto clamped = std::clamp (newScale, minimumInterfaceScale, maximumInterfaceScale);

    if (clamped == scale)
        return;

    scale = clamped;
    settings.setValue (settingKey::interfaceScale, std::to_string (scale));
    notify();
}

int Appearance::scaled (int logicalUnits) const
{
    return static_cast<int> (std::lround (logicalUnits * scale));
}

float Appearance::scaled (float logicalUnits) const
{
    return static_cast<float> (logicalUnits * scale);
}

void Appearance::resolve()
{
    const auto theme = resolveTheme (preference, desktopIsDark);

    // The preference itself is what the Interface tab shows, so a change to it
    // is worth telling the surfaces about even when the palette is unmoved —
    // Follow OS to Dark under a dark desktop, for one.
    resolved = theme;
    notify();
}

void Appearance::notify()
{
    // A copy, because a surface may take itself off the list as it is told.
    const auto told = listeners;

    for (auto* listener : told)
        listener->appearanceChanged();
}
} // namespace duet::gui
