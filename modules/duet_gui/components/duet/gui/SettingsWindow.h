#pragma once

#include <duet/gui/Appearance.h>
#include <duet/gui/Settings.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <filesystem>
#include <functional>

namespace duet::gui
{
/** The Settings window: everything app-global the producer sets once.

    It opens with one tab, Interface — theme, interface scale, projects
    directory, autosave interval, and the rendering escape hatch. Every row is
    in force where it stands and stored for the next launch.
*/
class SettingsWindow final : public juce::DocumentWindow
{
public:
    /** @param lookAndScale    the theme and the interface scale, read and written
        @param store            the app-global store the rows that are not the
                                appearance's are kept in
        @param defaultProjectsDirectory the location used before one is chosen
        @param renderingChanged called with the producer's choice of renderer, so
                                that the surfaces are moved onto it where they
                                stand
        @param onClose          called when the producer closes the window, so
                                that whoever opened it can drop it
    */
    SettingsWindow (Appearance& lookAndScale,
                    Settings& store,
                    const std::filesystem::path& defaultProjectsDirectory,
                    std::function<void (bool)> renderingChanged,
                    std::function<void()> onClose);

    ~SettingsWindow() override;

    void closeButtonPressed() override;

private:
    std::function<void()> closed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsWindow)
};
} // namespace duet::gui
