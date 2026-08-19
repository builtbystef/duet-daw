#pragma once

#include <duet/gui/Appearance.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace duet::gui
{
/** The Settings window: everything app-global the producer sets once.

    It opens with one tab, Interface — theme and interface scale — and later
    slices add their own beside it. Both of its rows write straight through the
    appearance, which is what puts them in force where they stand and stores them
    for the next launch.
*/
class SettingsWindow final : public juce::DocumentWindow
{
public:
    /** @param lookAndScale  what the window reads and writes
        @param onClose       called when the producer closes the window, so that
                             whoever opened it can drop it
    */
    SettingsWindow (Appearance& lookAndScale, std::function<void()> onClose);

    ~SettingsWindow() override;

    void closeButtonPressed() override;

private:
    std::function<void()> closed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsWindow)
};
} // namespace duet::gui
