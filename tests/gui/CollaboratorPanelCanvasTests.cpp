#include "GuiTestSupport.h"

#include <duet/gui/Appearance.h>
#include <duet/gui/ArrangementCanvas.h>
#include <duet/gui/CollaboratorPanelCanvas.h>
#include <duet/gui/MainShell.h>
#include <duet/gui/MixerCanvas.h>
#include <duet/gui/ViewState.h>
#include <duet/model/Session.h>

#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

using duet::gui::Appearance;
using duet::gui::CollaboratorPanelCanvas;
using duet::gui::Command;
using duet::gui::EntryKind;
using duet::gui::MainShell;
using duet::gui::noSelection;
using duet::gui::trackSelected;
using duet::gui::ViewState;
using duet::testing::StoredSettings;
using duet::testing::surfaceOf;

namespace
{
/** One Suggestion over the open project, as the surfaces read it.

    This suite links the interface and nothing under it, so the Suggestion
    manager cannot be named here — what is asserted is that the shell's three
    surfaces read one `Suggestions` and that a gesture on the card reaches the
    project. The mechanism behind the seam is the manager's own suite's, and
    what puts the manager here is `duet::app::SuggestionSurfaces`.
*/
class FabricatedSource final : public duet::gui::Suggestions::Source
{
public:
    FabricatedSource (duet::model::Session& openProject,
                      duet::model::TrackRef track,
                      duet::model::ClipRef clip)
        : session (openProject), keys (track), verse (clip)
    {
    }

    static constexpr double proposedLevelDb = -3.0;
    static constexpr double movedToSeconds = 4.0;

    [[nodiscard]] std::vector<duet::gui::SuggestionCardView> pending() override
    {
        if (resolved)
            return {};

        duet::gui::SuggestionCardView card;
        card.id = "sug-1";
        card.summary = "Rework the intro";

        duet::gui::SuggestionElementView moved;
        moved.description = "Move Verse later";
        moved.clips.push_back (duet::gui::GhostClip { keys, "Verse", movedToSeconds, 4.0, true });

        duet::gui::SuggestionElementView lifted;
        lifted.description = "Bring Keys up";
        lifted.faders.push_back (duet::gui::GhostFader { keys, proposedLevelDb });

        card.elements = { moved, lifted };

        return { card };
    }

    bool audition (std::string_view /*id*/, const std::vector<std::size_t>& elements) override
    {
        return session.auditionSuggestion (changes ("Rework the intro", elements));
    }

    void stopAudition() override { session.stopAudition(); }

    bool accept (std::string_view /*id*/, const std::vector<std::size_t>& elements) override
    {
        if (! session.acceptSuggestion (changes ("Rework the intro", elements)))
            return false;

        resolved = true;

        return true;
    }

    void reject (std::string_view /*id*/, const std::string& /*reason*/) override
    {
        resolved = true;
    }

    bool redo (std::string_view /*id*/) override
    {
        resolved = true;

        return true;
    }

private:
    [[nodiscard]] duet::model::Suggestion changes (std::string_view name,
                                                   const std::vector<std::size_t>& elements) const
    {
        duet::model::Suggestion applied { std::string { name } };

        for (const auto element : elements)
        {
            if (element == 0)
                applied.moveClip (verse, movedToSeconds);
            else
                applied.setTrackVolumeDb (keys, proposedLevelDb);
        }

        return applied;
    }

    duet::model::Session& session;
    duet::model::TrackRef keys;
    duet::model::ClipRef verse;
    bool resolved = false;
};

/** The window, with the Collaborator panel found inside it. */
struct OpenShell
{
    OpenShell()
    {
        shell.setBounds (0, 0, 1600, 980);

        auto* found = dynamic_cast<CollaboratorPanelCanvas*> (
            shell.findChildWithID (duet::gui::surfaceId::collaborator));

        REQUIRE (found != nullptr);
        panel = found;
    }

    [[nodiscard]] juce::Component& area (const char* areaId) const
    {
        auto* found = surfaceOf (*panel, areaId);

        REQUIRE (found != nullptr);
        return *found;
    }

    /** Clicks a button the way the producer does, and not on a later message:
        a test that clicks has to see the result now.
    */
    void click (const char* areaId) const
    {
        auto* button = dynamic_cast<juce::Button*> (&area (areaId));

        REQUIRE (button != nullptr);
        REQUIRE (button->isEnabled());
        button->onClick();
    }

    /** Typing into the composer. The editor's own change message arrives on a
        later pass of the message loop, which a test does not run, so this is
        what the producer's keystroke amounts to here.
    */
    void type (const juce::String& text) const
    {
        composer().setText (text);
        panel->refresh();
    }

    [[nodiscard]] juce::TextEditor& composer() const
    {
        auto* editor =
            dynamic_cast<juce::TextEditor*> (&area (duet::gui::collaboratorId::composer));

        REQUIRE (editor != nullptr);
        return *editor;
    }

    juce::ScopedJuceInitialiser_GUI juce;
    StoredSettings store;
    Appearance appearance { store, true };
    ViewState view;
    MainShell shell { appearance, view };
    CollaboratorPanelCanvas* panel = nullptr;
};

[[nodiscard]] std::filesystem::path editFile (const std::string& name)
{
    return std::filesystem::temp_directory_path()
           / ("duet-" + name + "-" + std::to_string (juce::Random::getSystemRandom().nextInt64())
              + ".tracktionedit");
}
} // namespace

TEST_CASE ("the right dock is the Collaborator panel, and C still opens and closes it")
{
    OpenShell open;

    REQUIRE (open.panel->isVisible());
    REQUIRE (open.composer().isEnabled());
    REQUIRE_FALSE (open.area (duet::gui::collaboratorId::conversation).getBounds().isEmpty());

    // The card is only there while a Task Run is.
    REQUIRE_FALSE (open.area (duet::gui::collaboratorId::taskRun).isVisible());

    open.shell.perform (Command::toggleCollaborator);

    REQUIRE_FALSE (open.panel->isVisible());
    REQUIRE_FALSE (open.view.collaboratorVisible());

    open.shell.perform (Command::toggleCollaborator);

    REQUIRE (open.panel->isVisible());
}

TEST_CASE ("the composer is a text field, so a bare letter typed into it is not a shortcut")
{
    const OpenShell open;
    const auto* typing = dynamic_cast<const juce::TextInputTarget*> (&open.composer());

    REQUIRE (typing != nullptr);
    REQUIRE (typing->isTextInputActive());
}

TEST_CASE ("sending from the composer chips the message with what the shell says is selected")
{
    OpenShell open;
    const auto file = editFile ("collaborator");
    duet::model::Session session { file };

    session.performAction (
        "Track",
        [] (auto& ops)
        { static_cast<void> (ops.createTrack (duet::model::TrackKind::midi, "Keys")); });
    open.shell.setSession (&session);

    REQUIRE (open.shell.currentSelectionContext() == noSelection());

    open.type ("Add a counter-melody");
    open.click (duet::gui::collaboratorId::send);

    const auto& conversation = open.panel->model().conversation();

    REQUIRE (conversation.size() == 1);
    REQUIRE (conversation.front().kind == EntryKind::producer);
    REQUIRE (conversation.front().text == "Add a counter-melody");
    REQUIRE (conversation.front().context.empty());
    REQUIRE (open.composer().getText().isEmpty());
    std::filesystem::remove (file);
}

TEST_CASE ("a message sent with a track under the producer's hand names that track")
{
    OpenShell open;
    const auto file = editFile ("collaborator-track");
    duet::model::Session session { file };
    duet::model::TrackRef track = duet::model::noTrack;

    session.performAction ("Track",
                           [&track] (auto& ops)
                           { track = ops.createTrack (duet::model::TrackKind::midi, "Keys"); });
    open.shell.setSession (&session);

    auto* arrangement = open.shell.findChildWithID (duet::gui::surfaceId::arrangement);
    REQUIRE (arrangement != nullptr);

    // The producer puts their hand on the track by clicking its lane.
    const auto position = juce::Point<float> { 245.0F, 140.0F };
    const auto now = juce::Time::getCurrentTime();
    const juce::MouseEvent click { juce::Desktop::getInstance().getMainMouseSource(),
                                   position,
                                   {},
                                   1.0F,
                                   0.0F,
                                   0.0F,
                                   0.0F,
                                   0.0F,
                                   arrangement,
                                   arrangement,
                                   now,
                                   position,
                                   now,
                                   1,
                                   false };
    arrangement->mouseDown (click);

    REQUIRE (open.shell.currentSelectionContext() == trackSelected ("Keys"));

    open.type ("Sit this in the mix");
    open.click (duet::gui::collaboratorId::send);

    REQUIRE (open.panel->model().conversation().back().context == "Keys");
    static_cast<void> (track);
    std::filesystem::remove (file);
}

TEST_CASE ("a long conversation scrolls to its newest message and leaves the composer where it is")
{
    OpenShell open;
    const auto composerBounds = open.composer().getBounds();

    for (auto message = 0; message < 40; ++message)
    {
        open.panel->model().beginTaskRun();
        open.panel->model().streamCommentary (
            "A line of commentary long enough to wrap more than once in a "
            "dock of this width, said again and again.",
            {});
        open.panel->model().finishTaskRun();
    }

    open.panel->refresh();

    const auto& conversation = open.area (duet::gui::collaboratorId::conversation);
    const auto* viewport = conversation.findParentComponentOfClass<juce::Viewport>();

    REQUIRE (viewport != nullptr);
    REQUIRE (conversation.getHeight() > viewport->getViewHeight());
    REQUIRE (viewport->getViewPositionY() + viewport->getViewHeight() >= conversation.getHeight());
    REQUIRE (open.composer().getBounds() == composerBounds);
}

TEST_CASE ("a Task Run shows the card, holds the composer, and Cancel ends it")
{
    OpenShell open;

    open.type ("Tighten this up");
    open.click (duet::gui::collaboratorId::send);
    open.panel->model().beginTaskRun();

    REQUIRE (open.panel->model().taskRunning());

    open.panel->refresh();

    REQUIRE (open.area (duet::gui::collaboratorId::taskRun).isVisible());
    REQUIRE_FALSE (open.area (duet::gui::collaboratorId::taskRun).getBounds().isEmpty());
    REQUIRE_FALSE (open.composer().isEnabled());

    open.click (duet::gui::collaboratorId::cancel);
    open.panel->refresh();

    REQUIRE_FALSE (open.panel->model().taskRunning());
    REQUIRE (open.composer().isEnabled());
    REQUIRE (open.panel->model().conversation().back().kind == EntryKind::notice);
    REQUIRE_FALSE (open.area (duet::gui::collaboratorId::taskRun).isVisible());
}

TEST_CASE ("a failed Task Run leaves the rest of the window working")
{
    OpenShell open;
    const auto file = editFile ("collaborator-failure");
    duet::model::Session session { file };

    open.shell.setSession (&session);

    const auto undoBefore = session.undoNames();

    // The shell holds no source of its own: what answers the producer is the
    // Collaborator the host wires in, so a run and its ending are driven here.
    open.type ("Tighten this up");
    open.click (duet::gui::collaboratorId::send);
    open.panel->model().beginTaskRun();
    open.panel->model().failTaskRun ("the Collaborator is not connected");
    open.panel->refresh();

    REQUIRE (open.panel->model().conversation().back().kind == EntryKind::failure);

    // Playback, editing and saving are untouched by a failure in the panel.
    REQUIRE (open.shell.keyPressed (juce::KeyPress { 'l' }));
    REQUIRE (session.isLooping());
    REQUIRE (session.undoNames() == undoBefore);
    REQUIRE (open.composer().isEnabled());
    std::filesystem::remove (file);
}

TEST_CASE ("the estimate mark is a thing the producer opens, and it opens onto the ledger")
{
    OpenShell open;

    open.panel->model().beginTaskRun();
    open.panel->model().streamCommentary ("It reads as C major.",
                                          { { "key", "C major", "krumhansl-schmuckler", 0.72 } });
    open.panel->model().finishTaskRun();
    open.panel->refresh();

    auto* mark = dynamic_cast<juce::Button*> (
        surfaceOf (*open.panel, duet::gui::collaboratorId::estimateMark));

    REQUIRE (mark != nullptr);

    const auto closed = open.area (duet::gui::collaboratorId::conversation).getHeight();

    mark->onClick();
    open.panel->refresh();

    // Opening the mark is what shows the ledger, so the conversation is taller
    // by the guesses it now lists.
    REQUIRE (open.panel->model().conversation().back().estimatesOpen);
    REQUIRE (open.area (duet::gui::collaboratorId::conversation).getHeight() > closed);
}

TEST_CASE ("Escape hands the keyboard back and leaves what the producer has typed")
{
    OpenShell open;
    auto& arrangement = *open.shell.findChildWithID (duet::gui::surfaceId::arrangement);

    // What the desktop tells the panel as the producer moves the keyboard from
    // the arrangement into the composer.
    open.panel->globalFocusChanged (&arrangement);
    open.panel->globalFocusChanged (&open.composer());
    open.type ("Half a thought");
    open.panel->returnFocusFromComposer();

    REQUIRE (open.panel->focusReturnSurface() == &arrangement);
    REQUIRE (open.composer().getText() == "Half a thought");
    REQUIRE (open.panel->model().conversation().empty());
}

TEST_CASE ("a quick prompt fills the composer and sends nothing")
{
    OpenShell open;
    auto* chip = dynamic_cast<juce::Button*> (&open.area (duet::gui::collaboratorId::quickPrompts));

    REQUIRE (chip != nullptr);

    chip->onClick();

    REQUIRE (open.composer().getText().isNotEmpty());
    REQUIRE (open.composer().getText()
             == juce::String { open.panel->model().quickPrompts().front() });
    REQUIRE (open.panel->model().conversation().empty());
}

TEST_CASE ("a run that succeeds comes back with a Suggestion, and every surface shows it")
{
    OpenShell open;
    const auto file = editFile ("collaborator-suggestion");
    duet::model::Session session { file };
    duet::model::TrackRef track = duet::model::noTrack;
    duet::model::ClipRef clip = duet::model::noClip;

    session.performAction ("Something to suggest over",
                           [&] (auto& ops)
                           {
                               track = ops.createTrack (duet::model::TrackKind::midi, "Keys");
                               clip = ops.insertMidiClip (track, "Verse", 0.0, 4.0);
                           });
    open.shell.setSession (&session);

    FabricatedSource source { session, track, clip };
    open.shell.pendingSuggestions().setSource (&source);

    // A run that succeeds comes back with a Suggestion, and its card goes in the
    // conversation the producer asked in.
    open.type ("Rework the intro");
    open.click (duet::gui::collaboratorId::send);
    open.panel->model().showSuggestion ("sug-1", "Rework the intro");
    open.panel->refresh();

    const auto& conversation = open.panel->model().conversation();
    const auto entry =
        std::find_if (conversation.begin(),
                      conversation.end(),
                      [] (const auto& said) { return said.kind == EntryKind::suggestion; });

    REQUIRE (entry != conversation.end());
    REQUIRE_FALSE (entry->suggestion.empty());

    // The card is in the conversation, the ghosts are on the timeline, and the
    // suggested level is beside the fader: all three read the one Suggestion the
    // shell hands them.
    REQUIRE (surfaceOf (*open.panel, duet::gui::collaboratorId::suggestionCard) != nullptr);

    auto* arrangement = dynamic_cast<duet::gui::ArrangementCanvas*> (
        open.shell.findChildWithID (duet::gui::surfaceId::arrangement));
    REQUIRE (arrangement != nullptr);
    REQUIRE_FALSE (arrangement->model().ghosts().empty());

    open.shell.perform (Command::showMixer);

    auto* mixer =
        dynamic_cast<duet::gui::MixerCanvas*> (surfaceOf (open.shell, duet::gui::surfaceId::mixer));
    REQUIRE (mixer != nullptr);
    REQUIRE (mixer->model().ghostFader (track).has_value());

    // Accepting from the card lands on the project as one Action, and takes the
    // ghosts down with it.
    const auto undoBefore = session.undoNames().size();

    open.click (duet::gui::collaboratorId::suggestionAccept);
    open.panel->refresh();

    REQUIRE (session.undoNames().size() == undoBefore + 1);
    REQUIRE (arrangement->model().ghosts().empty());
    REQUIRE_FALSE (mixer->model().ghostFader (track).has_value());
    std::filesystem::remove (file);
}
