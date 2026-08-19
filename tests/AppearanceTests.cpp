#include <duet/gui/Appearance.h>
#include <duet/gui/Settings.h>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <optional>
#include <string>
#include <string_view>

using duet::gui::Appearance;
using duet::gui::defaultInterfaceScale;
using duet::gui::Theme;
using duet::gui::ThemePreference;

namespace
{
/** The app-global store, held in memory. A second Appearance over the same one
    is the next launch: nothing about a restart matters to these tests except
    that the store outlives the object reading it.
*/
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

/** Counts what a surface would have been told. */
class ChangeCounter final : public Appearance::Listener
{
public:
    void appearanceChanged() override { ++changes; }

    int changes = 0;
};
} // namespace

TEST_CASE ("a first launch takes its theme from the desktop")
{
    StoredSettings store;

    SECTION ("a dark desktop")
    {
        const Appearance appearance { store, true };

        REQUIRE (appearance.themePreference() == ThemePreference::followSystem);
        REQUIRE (appearance.theme() == Theme::dark);
    }

    SECTION ("a light desktop")
    {
        const Appearance appearance { store, false };

        REQUIRE (appearance.theme() == Theme::light);
    }
}

TEST_CASE ("the producer's choice of Light outlives a restart under a dark desktop")
{
    StoredSettings store;

    {
        Appearance appearance { store, true };
        appearance.setThemePreference (ThemePreference::light);

        REQUIRE (appearance.theme() == Theme::light);
    }

    const Appearance nextLaunch { store, true };

    REQUIRE (nextLaunch.themePreference() == ThemePreference::light);
    REQUIRE (nextLaunch.theme() == Theme::light);
}

TEST_CASE ("Follow OS changes the theme when the desktop flips, with no restart")
{
    StoredSettings store;
    Appearance appearance { store, false };
    ChangeCounter surface;
    appearance.addListener (&surface);

    REQUIRE (appearance.theme() == Theme::light);

    appearance.systemDarkModeChanged (true);

    REQUIRE (appearance.theme() == Theme::dark);
    REQUIRE (surface.changes == 1);

    appearance.removeListener (&surface);
}

TEST_CASE ("a chosen theme ignores the desktop")
{
    StoredSettings store;
    Appearance appearance { store, false };
    appearance.setThemePreference (ThemePreference::dark);

    appearance.systemDarkModeChanged (false);

    REQUIRE (appearance.theme() == Theme::dark);
}

TEST_CASE ("a logical unit is a pixel at 1.0 and a quarter more by default")
{
    StoredSettings store;
    Appearance appearance { store, true };

    REQUIRE (appearance.interfaceScale() == defaultInterfaceScale);
    REQUIRE (appearance.scaled (24) == 30);

    appearance.setInterfaceScale (1.0);

    REQUIRE (appearance.scaled (24) == 24);
}

TEST_CASE ("a scale the producer changes lays the interface out again")
{
    StoredSettings store;
    Appearance appearance { store, true };
    ChangeCounter surface;
    appearance.addListener (&surface);

    appearance.setInterfaceScale (1.5);

    REQUIRE (appearance.scaled (24) == 36);
    REQUIRE (surface.changes == 1);

    // The same scale again is not a change, and a surface that laid itself out
    // for every set() would do it for every drag of the slider.
    appearance.setInterfaceScale (1.5);

    REQUIRE (surface.changes == 1);

    appearance.removeListener (&surface);
}

TEST_CASE ("a scale outside the offered range is brought into it")
{
    StoredSettings store;
    Appearance appearance { store, true };

    appearance.setInterfaceScale (8.0);

    REQUIRE (appearance.interfaceScale() == Appearance::maximumInterfaceScale);

    appearance.setInterfaceScale (0.1);

    REQUIRE (appearance.interfaceScale() == Appearance::minimumInterfaceScale);
}

TEST_CASE ("theme and interface scale round-trip through the store across a restart")
{
    StoredSettings store;

    {
        Appearance appearance { store, true };
        appearance.setThemePreference (ThemePreference::light);
        appearance.setInterfaceScale (1.5);
    }

    const Appearance nextLaunch { store, true };

    REQUIRE (nextLaunch.themePreference() == ThemePreference::light);
    REQUIRE (nextLaunch.interfaceScale() == 1.5);
}

TEST_CASE ("a stored preference the app does not know falls back to the desktop")
{
    StoredSettings store;
    store.setValue (duet::gui::settingKey::theme, "solarized");

    const Appearance appearance { store, true };

    REQUIRE (appearance.themePreference() == ThemePreference::followSystem);
    REQUIRE (appearance.theme() == Theme::dark);
}
