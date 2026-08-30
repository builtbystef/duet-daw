#include "GuiTestSupport.h"

#include <duet/gui/Appearance.h>
#include <duet/gui/AudioMidiSettings.h>
#include <duet/gui/Browser.h>
#include <duet/gui/Export.h>
#include <duet/gui/ExportDialog.h>
#include <duet/gui/PluginScan.h>
#include <duet/gui/PluginScanDialog.h>
#include <duet/gui/SettingsWindow.h>

#include <duet/model/Session.h>

#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include <filesystem>
#include <memory>
#include <string>

using duet::gui::Appearance;
using duet::gui::AudioMidiSettings;
using duet::gui::Browser;
using duet::gui::Export;
using duet::gui::ExportPanel;
using duet::gui::PluginScan;
using duet::gui::PluginScanPanel;
using duet::gui::SettingsPanel;
using duet::testing::StoredSettings;

/** The three dialogs of issue zm174o, as components.

    What each of them decides is its own view-model's, and asserted in the
    paintless suite; what this suite adds is what only a component knows — that
    the surface exists, that it drives the view-model beneath it, and that
    Escape dismisses it. Never what any of it looks like: paint stays untested
    (spec 535bbo).
*/
namespace
{
/** A project folder under the temp directory, taken away with this object. */
struct TempFolder
{
    TempFolder()
        : folder (std::filesystem::temp_directory_path()
                  / ("duet-dialog-tests-" + std::to_string (juce::Random().nextInt (1000000))))
    {
        std::filesystem::create_directories (folder);
    }

    ~TempFolder() { std::filesystem::remove_all (folder); }

    TempFolder (const TempFolder&) = delete;
    TempFolder& operator= (const TempFolder&) = delete;

    std::filesystem::path folder;
};

/** Everything the three dialogs read, over one real project. */
struct DialogFixture
{
    DialogFixture()
    {
        session.useNoAudioDevice();
        session.suppressDeviceRebuild();

        exporting.setProject (
            { &session, [] (duet::model::Session*) {} }, "Night Drive", temp.folder);
        scanning.setSession (&session);
        machine = std::make_unique<duet::gui::SessionAudioDevices> (session);
        audioAndMidi.setDevices (machine.get());
    }

    juce::ScopedJuceInitialiser_GUI juce;
    TempFolder temp;
    StoredSettings store;
    Appearance appearance { store, true };
    duet::model::Session session { temp.folder / "edit.tracktionedit" };
    Export exporting;
    PluginScan scanning;
    Browser dock { store };
    AudioMidiSettings audioAndMidi { store };
    std::unique_ptr<duet::gui::SessionAudioDevices> machine;
};

/** The Escape press each dialog is dismissed with.

    Made where it is used: `juce::KeyPress::escapeKey` is itself a non-local
    constant, and one non-local reading another before main is an order nobody
    states.
*/
[[nodiscard]] juce::KeyPress escape() { return juce::KeyPress { juce::KeyPress::escapeKey }; }
} // namespace

//==============================================================================
TEST_CASE ("the Export dialog is dismissed from the keyboard")
{
    DialogFixture fixture;
    bool dismissed = false;

    ExportPanel panel { fixture.appearance, fixture.exporting, [&dismissed] { dismissed = true; } };
    panel.setBounds (0, 0, 500, 400);

    REQUIRE (panel.keyPressed (escape()));
    REQUIRE (dismissed);
}

TEST_CASE ("dismissing the Export dialog abandons the render it was watching")
{
    DialogFixture fixture;
    ExportPanel panel { fixture.appearance, fixture.exporting, [] {} };
    panel.setBounds (0, 0, 500, 400);

    // A project with nothing in it is one bar long, which is a range something
    // can be written from.
    panel.beginExport();

    REQUIRE (fixture.exporting.isRunning());
    REQUIRE (panel.statusText().isNotEmpty());

    static_cast<void> (panel.keyPressed (escape()));

    // The producer waits for nothing here: the render is asked to stop, and it
    // stops between blocks.
    for (int attempt = 0; attempt < 200 && fixture.exporting.isRunning(); ++attempt)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (10);

    REQUIRE_FALSE (fixture.exporting.isRunning());
    REQUIRE (fixture.exporting.state() == duet::gui::ExportState::cancelled);
}

TEST_CASE ("the plugin-scan dialog is dismissed from the keyboard")
{
    DialogFixture fixture;
    bool dismissed = false;

    PluginScanPanel panel { fixture.appearance, fixture.scanning, [&dismissed] {
                               dismissed = true;
                           } };
    panel.setBounds (0, 0, 500, 400);

    REQUIRE (panel.keyPressed (escape()));
    REQUIRE (dismissed);
}

TEST_CASE ("the plugin-scan dialog says so when there is nowhere to scan")
{
    DialogFixture fixture;
    fixture.scanning.setDirectories ({ fixture.temp.folder / "there-is-no-such-folder" });

    PluginScanPanel panel { fixture.appearance, fixture.scanning, [] {} };
    panel.setBounds (0, 0, 500, 400);

    panel.beginScan();

    // The producer is told, rather than watching a bar that never moves. What a
    // scan of a real directory says while it runs is the view-model's, and is
    // asserted in the paintless suite: this suite links no VST3 host.
    REQUIRE (panel.statusText() == "Nowhere to scan");
    REQUIRE (panel.resultLines().empty());
}

TEST_CASE ("the Settings window holds Interface, Audio and MIDI, and is keyboard-dismissible")
{
    DialogFixture fixture;
    bool dismissed = false;

    SettingsPanel panel { fixture.appearance,
                          fixture.store,
                          fixture.dock,
                          fixture.audioAndMidi,
                          fixture.temp.folder,
                          {},
                          [&dismissed] { dismissed = true; } };
    panel.setBounds (0, 0, 500, 600);

    const auto names = panel.tabNames();

    REQUIRE (names.size() == 3);
    REQUIRE (names[0] == "Interface");
    REQUIRE (names[1] == "Audio");
    REQUIRE (names[2] == "MIDI");

    // Audio & MIDI Settings from the Duet menu lands on the Audio tab.
    panel.showTab (SettingsPanel::audioTab);
    REQUIRE (panel.currentTab() == SettingsPanel::audioTab);

    REQUIRE (panel.keyPressed (escape()));
    REQUIRE (dismissed);
}
