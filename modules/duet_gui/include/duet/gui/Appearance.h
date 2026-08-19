#pragma once

#include <duet/gui/Tokens.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace duet::gui
{
class Settings;

/** Which theme the producer asked for, which is not the same question as which
    one is in force: `followSystem` hands the answer to the desktop.
*/
enum class ThemePreference : std::uint8_t
{
    followSystem,
    dark,
    light
};

/** The keys the app-global store holds the interface's settings under. Named
    because the store is one namespace every slice adds to.
*/
namespace settingKey
{
    inline constexpr std::string_view theme = "interface.theme";
    inline constexpr std::string_view interfaceScale = "interface.scale";
} // namespace settingKey

/** The palette in force, given what the producer asked for and what the desktop
    is set to now.
*/
[[nodiscard]] Theme resolveTheme (ThemePreference preference, bool systemIsDark);

/** What one logical unit is worth in pixels until the producer says otherwise.

    A quarter more than 1:1: the visual reference's pixel sizes read too small at
    1:1 on a workstation display, which is what the r4m858 prototype settled.
*/
inline constexpr double defaultInterfaceScale = 1.25;

/** The look the whole interface is drawn with: which palette is in force, and
    how big a logical unit is.

    One of these exists for the run of the app. Every surface reads its colours
    and its measurements through it and listens for the changes, so a theme or a
    scale the producer changes in Settings re-lays out what is already on screen
    rather than waiting for a restart.

    Both values are the producer's and outlive the session: they are read from
    the app-global store as this is made, and written back as they change.
*/
class Appearance
{
public:
    /** Reads the producer's stored choices, and takes the desktop's current dark
        setting for the first resolution.
    */
    Appearance (Settings& store, bool systemIsDark);

    ~Appearance();

    Appearance (const Appearance& other) = delete;
    Appearance& operator= (const Appearance& other) = delete;

    /** What a surface implements to be told the look has changed under it. */
    class Listener
    {
    public:
        virtual ~Listener() = default;

        Listener (const Listener& other) = delete;
        Listener& operator= (const Listener& other) = delete;

        virtual void appearanceChanged() = 0;

    protected:
        Listener() = default;
    };

    void addListener (Listener* listener);
    void removeListener (Listener* listener);

    [[nodiscard]] ThemePreference themePreference() const { return preference; }

    /** The producer's choice, in force at once and stored for the next launch. */
    void setThemePreference (ThemePreference newPreference);

    /** The palette every surface paints with. */
    [[nodiscard]] Theme theme() const { return resolved; }

    /** The value of a token under the palette in force. */
    [[nodiscard]] Colour colour (ColourToken token) const
    {
        return duet::gui::colour (token, resolved);
    }

    /** Tells the appearance what the desktop is set to now. A flip while the
        preference is `followSystem` changes the theme where it stands.
    */
    void systemDarkModeChanged (bool systemIsDark);

    [[nodiscard]] double interfaceScale() const { return scale; }

    /** The producer's choice, clamped to the range the setting offers. */
    void setInterfaceScale (double newScale);

    /** A measurement in logical units, in pixels. */
    [[nodiscard]] int scaled (int logicalUnits) const;
    [[nodiscard]] float scaled (float logicalUnits) const;

    /** The range the Interface tab offers: small enough for a dense workstation
        on a large display, large enough to read on a laptop.
    */
    static constexpr double minimumInterfaceScale = 0.75;
    static constexpr double maximumInterfaceScale = 2.0;

private:
    void resolve();
    void notify();

    Settings& settings;
    bool desktopIsDark;
    ThemePreference preference;
    double scale;
    Theme resolved;
    std::vector<Listener*> listeners;
};
} // namespace duet::gui
