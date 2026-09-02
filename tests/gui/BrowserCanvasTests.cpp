#include "GuiTestSupport.h"

#include <duet/gui/Appearance.h>
#include <duet/gui/ArrangementCanvas.h>
#include <duet/gui/ArrangementView.h>
#include <duet/gui/Browser.h>
#include <duet/gui/BrowserCanvas.h>
#include <duet/gui/MainShell.h>
#include <duet/gui/MixerCanvas.h>
#include <duet/gui/ViewState.h>

#include <duet/model/Session.h>

#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>

using duet::gui::Appearance;
using duet::gui::ArrangementCanvas;
using duet::gui::ArrangementView;
using duet::gui::Browser;
using duet::gui::BrowserCanvas;
using duet::gui::BrowserSectionKind;
using duet::gui::MainShell;
using duet::gui::Mixer;
using duet::gui::MixerCanvas;
using duet::gui::SourceAudition;
using duet::gui::SourceAuditionState;
using duet::gui::ViewState;
using duet::model::Session;
using duet::model::TrackKind;
using duet::testing::StoredSettings;
using duet::testing::surfaceOf;

namespace
{
/** A project folder under the system temp directory, gone with this object. */
class TempFolder
{
public:
    TempFolder()
        : folder (std::filesystem::temp_directory_path()
                  / ("duet-gui-" + juce::Uuid {}.toString().toStdString()))
    {
        std::filesystem::create_directories (folder);
    }

    ~TempFolder() { std::filesystem::remove_all (folder); }

    TempFolder (const TempFolder&) = delete;
    TempFolder& operator= (const TempFolder&) = delete;

    [[nodiscard]] std::filesystem::path editFile() const { return folder / "project.duet"; }

private:
    std::filesystem::path folder;
};

/** What a drag out of the browser looks like to a surface it is dropped on. */
[[nodiscard]] juce::DragAndDropTarget::SourceDetails dropOf (const std::string& identity,
                                                             juce::Point<int> where)
{
    return { duet::gui::browserDrag::descriptionOf (identity), nullptr, where };
}

/** The identity of one of the browser's items, by the name it shows. */
[[nodiscard]] std::string
    identityOf (const Browser& browser, BrowserSectionKind kind, const std::string& name)
{
    for (const auto& section : browser.sections())
        if (section.kind == kind)
            for (const auto& item : section.items)
                if (item.name == name)
                    return item.identity;

    return {};
}
} // namespace

TEST_CASE ("the shell's left dock is the browser, and it can start a drag")
{
    const juce::ScopedJuceInitialiser_GUI juce;
    StoredSettings store;
    Appearance appearance { store, true };
    ViewState view;
    MainShell shell { appearance, view, store };
    shell.setBounds (0, 0, 1600, 980);

    auto* dock = surfaceOf (shell, duet::gui::surfaceId::browser);
    REQUIRE (dock != nullptr);
    REQUIRE (dynamic_cast<BrowserCanvas*> (dock) != nullptr);

    // A drag has to have a container over it to be carried anywhere, and the
    // shell is what the browser, the arrangement and the mixer all sit under.
    REQUIRE (juce::DragAndDropContainer::findParentDragContainerFor (dock) == &shell);
    REQUIRE_FALSE (shell.browser().sections().empty());
}

TEST_CASE ("a device dragged onto a track lands on the track under the pointer")
{
    const juce::ScopedJuceInitialiser_GUI juce;
    const TempFolder temp;
    Session session { temp.editFile() };
    StoredSettings store;
    Appearance appearance { store, true };
    ViewState view;
    ArrangementView arrangement { view };
    Browser browser { store };
    browser.setSession (&session);
    arrangement.setSession (&session);

    duet::model::TrackRef track = duet::model::noTrack;
    session.performAction (
        "Track", [&] (auto& ops) { track = ops.createTrack (TrackKind::audio, "Audio"); });

    ArrangementCanvas canvas { appearance, arrangement };
    canvas.setBrowser (&browser);
    canvas.setBounds (0, 0, 1200, 600);

    const auto reverb = identityOf (browser, BrowserSectionKind::effects, "Reverb");
    REQUIRE_FALSE (reverb.empty());

    const auto rows = arrangement.tracks();
    REQUIRE (rows.back().track == track);
    const juce::Point<int> onTheTrack {
        appearance.scaled (ArrangementCanvas::trackHeaderWidth) + 40,
        appearance.scaled (ArrangementCanvas::rulerHeight) + rows.back().y + 4
    };

    REQUIRE (canvas.isInterestedInDragSource (dropOf (reverb, onTheTrack)));
    canvas.itemDropped (dropOf (reverb, onTheTrack));

    REQUIRE (session.track (track).plugins.size() == 1);
    REQUIRE (session.track (track).plugins.front().builtin == duet::model::BuiltinPlugin::reverb);

    // Something the browser never handed out is not a browser drag, and a drop
    // below the last track has no track to land on: neither is an Action.
    const auto actions = session.undoNames().size();
    REQUIRE_FALSE (
        canvas.isInterestedInDragSource ({ juce::var { "somethingElse" }, nullptr, onTheTrack }));
    canvas.itemDropped (dropOf (reverb, { onTheTrack.x, canvas.getHeight() - 1 }));
    REQUIRE (session.undoNames().size() == actions);
}

TEST_CASE ("a device dragged into a strip's insert chain lands where it was dropped")
{
    const juce::ScopedJuceInitialiser_GUI juce;
    const TempFolder temp;
    Session session { temp.editFile() };
    StoredSettings store;
    Appearance appearance { store, true };
    Mixer mixer;
    Browser browser { store };
    browser.setSession (&session);
    mixer.setSession (&session);

    // The strip the drop lands on is the leftmost one, which is the project's
    // own first track: where a strip is drawn is the canvas's, and the test
    // asks about the place in the chain rather than about the arithmetic.
    REQUIRE_FALSE (mixer.strips().empty());
    const auto track = mixer.strips().front().channel;
    session.performAction ("Chain",
                           [&] (auto& ops)
                           {
                               ops.addPlugin (track, duet::model::BuiltinPlugin::eq, 0);
                               ops.addPlugin (track, duet::model::BuiltinPlugin::compressor, 1);
                           });

    MixerCanvas canvas { appearance, mixer, {}, [] {} };
    canvas.setBrowser (&browser);
    canvas.setBounds (0, 0, 800, 400);

    const auto reverb = identityOf (browser, BrowserSectionKind::effects, "Reverb");
    REQUIRE_FALSE (reverb.empty());

    // The second row of the chain is the place between the two plugins there.
    canvas.itemDropped (dropOf (reverb, { 8, appearance.scaled (112 + 20) + 2 }));

    const auto plugins = session.track (track).plugins;
    REQUIRE (plugins.size() == 3);
    REQUIRE (plugins[1].builtin == duet::model::BuiltinPlugin::reverb);
}

TEST_CASE ("the browser canvas composes scanning status from the model")
{
    const juce::ScopedJuceInitialiser_GUI juce;
    StoredSettings store;
    Appearance appearance { store, true };
    Browser browser { store };
    browser.setScanWorker ([] (const auto&) {});

    BrowserCanvas canvas { appearance, browser };
    canvas.setBounds (0, 0, 280, 600);
    REQUIRE (canvas.composedStatus().empty());

    browser.addSampleFolder ("not-a-real-folder");
    REQUIRE (canvas.composedStatus() == "Scanning… 0/1");
}

namespace
{
class FakeSourceAudition final : public SourceAudition
{
public:
    void play (std::filesystem::path file, std::string identity) override
    {
        current.identity = std::move (identity);
        current.state = SourceAuditionState::playing;
        playing = std::move (file);
        ++plays;
        notify();
    }

    void stop() override
    {
        playing.reset();
        current = {};
        ++stops;
        notify();
    }

    [[nodiscard]] duet::gui::SourceAuditionStatus status() const override { return current; }
    void onChanged (std::function<void()> callback) override { changed = std::move (callback); }

    std::optional<std::filesystem::path> playing;
    duet::gui::SourceAuditionStatus current;
    int plays = 0;
    int stops = 0;
    std::function<void()> changed;

private:
    void notify() const
    {
        if (changed)
            changed();
    }
};

void writeSample (const std::filesystem::path& folder, const std::string& fileName)
{
    std::filesystem::create_directories (folder);
    std::ofstream stream { folder / fileName, std::ios::binary };
    stream << "not really audio";
}
} // namespace

TEST_CASE ("Browser-focused Space and the Play button toggle Source audition")
{
    const juce::ScopedJuceInitialiser_GUI juce;
    const TempFolder temp;
    StoredSettings store;
    Appearance appearance { store, true };
    Browser browser { store };
    FakeSourceAudition audition;
    const auto loops = temp.editFile().parent_path() / "loops";
    writeSample (loops, "kick.wav");
    browser.addSampleFolder (loops);
    browser.setSourceAudition (&audition);

    BrowserCanvas canvas { appearance, browser };
    canvas.setBounds (0, 0, 280, 600);
    REQUIRE (canvas.auditionButtonText() == "Play");
    REQUIRE (canvas.composedAudition().state == SourceAuditionState::stopped);

    const auto kick = identityOf (browser, BrowserSectionKind::samples, "kick.wav");
    REQUIRE_FALSE (kick.empty());
    browser.select (kick);

    REQUIRE (canvas.keyPressed (juce::KeyPress { juce::KeyPress::spaceKey }));
    REQUIRE (audition.plays == 1);
    REQUIRE (audition.current.identity == kick);
    REQUIRE (canvas.auditionButtonText() == "Stop");
    REQUIRE (canvas.composedAudition().state == SourceAuditionState::playing);

    REQUIRE (canvas.keyPressed (juce::KeyPress { juce::KeyPress::spaceKey }));
    REQUIRE (audition.stops == 1);
    REQUIRE (canvas.auditionButtonText() == "Play");
    REQUIRE (canvas.composedAudition().state == SourceAuditionState::stopped);
}

TEST_CASE ("Space in the search box is typing, not Source audition")
{
    const juce::ScopedJuceInitialiser_GUI juce;
    StoredSettings store;
    Appearance appearance { store, true };
    Browser browser { store };
    FakeSourceAudition audition;
    browser.setSourceAudition (&audition);

    BrowserCanvas canvas { appearance, browser };
    canvas.setBounds (0, 0, 280, 600);

    juce::TextEditor* search = nullptr;

    for (auto* child : canvas.getChildren())
        if (auto* editor = dynamic_cast<juce::TextEditor*> (child); editor != nullptr)
            search = editor;

    REQUIRE (search != nullptr);
    search->grabKeyboardFocus();

    REQUIRE (search->hasKeyboardFocus (true));
    REQUIRE_FALSE (canvas.hasKeyboardFocus (false));
    REQUIRE (audition.plays == 0);
}
