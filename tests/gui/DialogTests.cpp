#include "GuiTestSupport.h"

#include <duet/gui/Appearance.h>
#include <duet/gui/AudioMidiSettings.h>
#include <duet/gui/Browser.h>
#include <duet/gui/Export.h>
#include <duet/gui/ExportDialog.h>
#include <duet/gui/ModelPicker.h>
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
using duet::gui::ModelPicker;
using duet::gui::PluginScan;
using duet::gui::PluginScanPanel;
using duet::gui::SettingsPanel;
using duet::testing::StoredSettings;
using duet::testing::surfaceOf;

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
    ModelPicker collaboratorModels { store };
    std::unique_ptr<duet::gui::SessionAudioDevices> machine;
};

/** The provider layer, held in memory: what the Collaborator tab is driven
    against, so that the surface is exercised with no sidecar and no provider.

    The rules it stands for are the picker's own suite's; what this one adds is
    what only a component knows — that the tab is there, that its rows reach the
    picker, and that an entry nothing authenticates cannot be chosen.
*/
class FakeAccess final : public ModelPicker::Source
{
public:
    bool authenticated = false;

    Listing listing() override
    {
        Listing out;
        out.providers.push_back ({ "openai", "OpenAI", true, true, authenticated, authenticated });
        out.providers.push_back ({ "anthropic", "Anthropic", true, true, false, false });

        if (authenticated)
            out.models.push_back (
                { "openai:gpt-5.6-terra", "GPT-5.6 Terra", "openai", "OpenAI", true });

        return out;
    }

    std::string setApiKey (const std::string& provider, const std::string& key) override
    {
        asked = provider;

        if (key.empty())
            return "Enter a key.";

        authenticated = true;

        return {};
    }

    std::string removeCredentials (const std::string& provider) override
    {
        asked = provider;
        authenticated = false;

        return {};
    }

    std::string beginOAuth (const std::string& provider, duet::gui::OAuthStep& step) override
    {
        asked = provider;
        step = { "https://example.invalid/sign-in", "Sign in there." };

        return {};
    }

    std::string completeOAuth (const std::string& provider, const std::string& code) override
    {
        asked = provider;
        pasted = code;

        return {};
    }

    void useModel (const std::string& modelId) override { used = modelId; }

    /** The provider the last gesture was about, what was pasted into a sign-in,
        and the model a run would use: what the tab said, in the order it said
        it.
    */
    std::string asked;
    std::string pasted;
    std::string used;
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

TEST_CASE ("the component suite hosts VST3, as the paintless suite does")
{
    const DialogFixture fixture;

    // juce_audio_processors is an INTERFACE source: it compiles into every
    // target that links it, so what duet::gui_components defines about hosting
    // has to be what duet::model defines. Two answers to this question is an
    // ODR violation in duet_app rather than a build error (issue 4v2m38), and
    // the paintless suite's answer — PluginHostingTests — is the one to match.
    REQUIRE (fixture.session.canHostVst3());
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
    // asserted in the paintless suite; what this suite adds is the surface.
    REQUIRE (panel.statusText() == "Nowhere to scan");
    REQUIRE (panel.resultLines().empty());
}

TEST_CASE ("the Settings window holds its four tabs, and is keyboard-dismissible")
{
    DialogFixture fixture;
    bool dismissed = false;

    SettingsPanel panel { fixture.appearance,
                          fixture.store,
                          fixture.dock,
                          fixture.audioAndMidi,
                          fixture.collaboratorModels,
                          fixture.temp.folder,
                          {},
                          [&dismissed] { dismissed = true; } };
    panel.setBounds (0, 0, 500, 600);

    const auto names = panel.tabNames();

    REQUIRE (names.size() == 4);
    REQUIRE (names[0] == "Interface");
    REQUIRE (names[1] == "Audio");
    REQUIRE (names[2] == "MIDI");
    REQUIRE (names[3] == "Collaborator");

    // Audio & MIDI Settings from the Duet menu lands on the Audio tab.
    panel.showTab (SettingsPanel::audioTab);
    REQUIRE (panel.currentTab() == SettingsPanel::audioTab);

    REQUIRE (panel.keyPressed (escape()));
    REQUIRE (dismissed);
}

TEST_CASE ("a key entered in the Collaborator tab makes a model choosable")
{
    DialogFixture fixture;
    FakeAccess access;
    fixture.collaboratorModels.setSource (&access);

    SettingsPanel panel {
        fixture.appearance,         fixture.store,       fixture.dock, fixture.audioAndMidi,
        fixture.collaboratorModels, fixture.temp.folder, {},           [] {}
    };
    panel.setBounds (0, 0, 500, 600);
    panel.showTab (SettingsPanel::collaboratorTab);

    auto* models = dynamic_cast<juce::ComboBox*> (surfaceOf (panel, duet::gui::settingsId::model));
    auto* key = dynamic_cast<juce::TextEditor*> (surfaceOf (panel, duet::gui::settingsId::apiKey));
    auto* save = dynamic_cast<juce::Button*> (surfaceOf (panel, duet::gui::settingsId::saveApiKey));
    auto* remove =
        dynamic_cast<juce::Button*> (surfaceOf (panel, duet::gui::settingsId::removeCredentials));

    REQUIRE (models != nullptr);
    REQUIRE (key != nullptr);
    REQUIRE (save != nullptr);
    REQUIRE (remove != nullptr);

    // Nothing set up: nothing to choose from, and the box says why rather than
    // standing empty.
    REQUIRE (models->getNumItems() == 0);
    REQUIRE (models->getTextWhenNothingSelected().isNotEmpty());

    key->setText ("sk-duet-1");
    save->onClick();

    REQUIRE (models->getNumItems() == 1);
    REQUIRE (models->getItemText (0).contains ("OpenAI"));
    REQUIRE (fixture.collaboratorModels.selectedModel() == ModelPicker::recommendedModel);
    REQUIRE (access.used == ModelPicker::recommendedModel);

    // The key is not left on the screen for whoever is behind the producer.
    REQUIRE (key->getText().isEmpty());

    remove->onClick();

    REQUIRE (fixture.collaboratorModels.selectedModel().empty());
    REQUIRE (models->getNumItems() == 0);
}

TEST_CASE ("an unauthenticated model is offered in the Collaborator tab and cannot be chosen")
{
    DialogFixture fixture;
    FakeAccess access;
    access.authenticated = true;
    fixture.collaboratorModels.setSource (&access);

    SettingsPanel panel {
        fixture.appearance,         fixture.store,       fixture.dock, fixture.audioAndMidi,
        fixture.collaboratorModels, fixture.temp.folder, {},           [] {}
    };
    panel.setBounds (0, 0, 500, 600);
    panel.showTab (SettingsPanel::collaboratorTab);

    auto* models = dynamic_cast<juce::ComboBox*> (surfaceOf (panel, duet::gui::settingsId::model));

    REQUIRE (models != nullptr);
    REQUIRE (models->getNumItems() == 1);
    REQUIRE (models->isItemEnabled (1));
}

TEST_CASE ("the Collaborator tab shows the address a subscription sign-in returned")
{
    DialogFixture fixture;
    FakeAccess access;
    fixture.collaboratorModels.setSource (&access);

    SettingsPanel panel {
        fixture.appearance,         fixture.store,       fixture.dock, fixture.audioAndMidi,
        fixture.collaboratorModels, fixture.temp.folder, {},           [] {}
    };
    panel.setBounds (0, 0, 500, 600);
    panel.showTab (SettingsPanel::collaboratorTab);

    auto* signIn = dynamic_cast<juce::Button*> (surfaceOf (panel, duet::gui::settingsId::signIn));
    auto* instructions =
        dynamic_cast<juce::TextEditor*> (surfaceOf (panel, duet::gui::settingsId::instructions));

    REQUIRE (signIn != nullptr);
    REQUIRE (instructions != nullptr);
    REQUIRE (instructions->getText().isEmpty());

    signIn->onClick();

    // What the sidecar returned, both halves of it, in front of the producer.
    REQUIRE (instructions->getText().contains ("https://example.invalid/sign-in"));
    REQUIRE (instructions->getText().contains ("Sign in there."));
}
