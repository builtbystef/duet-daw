#pragma once

#include <duet/gui/Appearance.h>
#include <duet/gui/AudioMidiSettings.h>
#include <duet/gui/Browser.h>
#include <duet/gui/ModelPicker.h>
#include <duet/gui/Settings.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

namespace duet::gui
{
/** The names the Collaborator tab's controls carry, so that a test can find
    one.
*/
namespace settingsId
{
    inline constexpr const char* model = "settingsModel";
    inline constexpr const char* provider = "settingsProvider";
    inline constexpr const char* apiKey = "settingsApiKey";
    inline constexpr const char* saveApiKey = "settingsSaveApiKey";
    inline constexpr const char* signIn = "settingsSignIn";
    inline constexpr const char* oauthCode = "settingsOAuthCode";
    inline constexpr const char* finishSignIn = "settingsFinishSignIn";
    inline constexpr const char* removeCredentials = "settingsRemoveCredentials";
    inline constexpr const char* instructions = "settingsSignInInstructions";
    inline constexpr const char* status = "settingsCollaboratorStatus";
} // namespace settingsId

/** The Settings window: everything app-global the producer sets once.

    Four tabs. Interface — theme, interface scale, projects directory, autosave
    interval, the browser's sample folders, and the rendering escape hatch. Audio
    — the output and input device, the rate, the buffer, and the latency they
    add up to. MIDI — the machine's MIDI inputs, and which of them are switched
    on. Collaborator — the model a Task Run uses, and the providers the producer
    reaches it through. Every row is in force where it stands and stored for the
    next launch.
*/
/** The Settings window's surface: the four tabs, in the appearance they are
    measured in.

    A surface and not a window, for the reason the other two dialogs have one:
    what a window is is a peer on a screen, and what this does is the same on a
    machine that has none.
*/
class SettingsPanel final : public juce::Component
{
public:
    SettingsPanel (Appearance& lookAndScale,
                   Settings& store,
                   Browser& dock,
                   AudioMidiSettings& machine,
                   ModelPicker& collaboratorModels,
                   const std::filesystem::path& defaultProjectsDirectory,
                   std::function<void (bool)> renderingChanged,
                   std::function<void()> onClose);

    ~SettingsPanel() override;

    SettingsPanel (const SettingsPanel& other) = delete;
    SettingsPanel& operator= (const SettingsPanel& other) = delete;

    void resized() override;

    /** Escape dismisses it, like every other dialog here. */
    bool keyPressed (const juce::KeyPress& key) override;

    /** Puts it on one tab, and reads the machine again as it does: the device
        the producer is looking at is the device that is running now.
    */
    void showTab (int index);

    [[nodiscard]] int currentTab() const;

    /** What the tabs are called, in the order they are offered. */
    [[nodiscard]] std::vector<juce::String> tabNames() const;

    static constexpr int interfaceTab = 0;
    static constexpr int audioTab = 1;
    static constexpr int midiTab = 2;
    static constexpr int collaboratorTab = 3;

private:
    class Tabs;

    std::unique_ptr<Tabs> tabs;
    std::function<void()> dismiss;
};

class SettingsWindow final : public juce::DocumentWindow
{
public:
    /** @param lookAndScale    the theme and the interface scale, read and written
        @param store            the app-global store the rows that are not the
                                appearance's are kept in
        @param dock             the browser whose sample folders this tab
                                manages, so that a folder added here is in the
                                dock before the window is closed
        @param machine          the machine's audio and MIDI hardware, as the
                                Audio and MIDI tabs set it
        @param collaboratorModels what the Collaborator tab sets up: the model a
                                Task Run uses and the providers behind it
        @param defaultProjectsDirectory the location used before one is chosen
        @param renderingChanged called with the producer's choice of renderer, so
                                that the surfaces are moved onto it where they
                                stand
        @param onClose          called when the producer closes the window, so
                                that whoever opened it can drop it
    */
    SettingsWindow (Appearance& lookAndScale,
                    Settings& store,
                    Browser& dock,
                    AudioMidiSettings& machine,
                    ModelPicker& collaboratorModels,
                    const std::filesystem::path& defaultProjectsDirectory,
                    std::function<void (bool)> renderingChanged,
                    std::function<void()> onClose);

    ~SettingsWindow() override;

    void closeButtonPressed() override;

    /** Opens the window on one of its tabs, so that Audio & MIDI Settings from
        the Duet menu lands where the producer asked to be.
    */
    void showTab (int index);

    static constexpr int interfaceTab = SettingsPanel::interfaceTab;
    static constexpr int audioTab = SettingsPanel::audioTab;
    static constexpr int midiTab = SettingsPanel::midiTab;
    static constexpr int collaboratorTab = SettingsPanel::collaboratorTab;

private:
    SettingsPanel* panel = nullptr;
    std::function<void()> closed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsWindow)
};
} // namespace duet::gui
