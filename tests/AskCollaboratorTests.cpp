#include <duet/app/OpeningContext.h>

#include <duet/collab/ProjectTools.h>
#include <duet/collab/TaskRun.h>

#include <duet/gui/ArrangementView.h>
#include <duet/gui/CollaboratorPanel.h>
#include <duet/gui/Selection.h>
#include <duet/gui/ViewState.h>

#include <duet/model/Session.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using duet::gui::ArrangementView;
using duet::gui::AskScope;
using duet::gui::SelectionKind;
using duet::gui::ViewState;
using duet::model::ClipRef;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;

namespace
{
/** A project with three tracks and three named clips on the first of them: enough
    for a producer to have one thing selected and ask about another.
*/
struct AskArrangement
{
    AskArrangement() : session (project.editFile())
    {
        arrangement.setSession (&session);
        arrangement.setWidthPx (800);
        view.setHZoomPxPerBeat (20.0);
        session.performAction ("Clips",
                               [&] (auto& ops)
                               {
                                   drums = ops.createTrack (TrackKind::midi, "Drums");
                                   keys = ops.createTrack (TrackKind::midi, "Keys");
                                   bass = ops.createTrack (TrackKind::midi, "Bass");
                                   verse = ops.insertMidiClip (drums, "Verse", 0.0, 1.0);
                                   drop = ops.insertMidiClip (drums, "Drop", 4.0, 1.0);
                                   outro = ops.insertMidiClip (drums, "Outro", 8.0, 1.0);
                               });
    }

    void select (const std::vector<ClipRef>& clips)
    {
        arrangement.selection().clear();
        arrangement.selection().focus (SelectionKind::clip);

        for (const auto clip : clips)
            arrangement.selection().click (
                { SelectionKind::clip, clip }, arrangement.allClipItems(), true, false);
    }

    TempProject project;
    Session session;
    ViewState view;
    ArrangementView arrangement { view };
    TrackRef drums = duet::model::noTrack;
    TrackRef keys = duet::model::noTrack;
    TrackRef bass = duet::model::noTrack;
    ClipRef verse = duet::model::noClip;
    ClipRef drop = duet::model::noClip;
    ClipRef outro = duet::model::noClip;
};
} // namespace

TEST_CASE ("asking about a clip outside the selection makes that clip the context")
{
    AskArrangement project;

    project.select ({ project.verse });
    project.arrangement.askAboutClip (project.drop);

    const auto asked = project.arrangement.askContext();

    REQUIRE (asked.scope == AskScope::clips);
    REQUIRE (asked.clips == std::vector<ClipRef> { project.drop });
    REQUIRE (asked.name == "Drop");
}

TEST_CASE ("asking about a clip inside the selection keeps the whole selection")
{
    AskArrangement project;

    project.select ({ project.verse, project.outro });
    project.arrangement.askAboutClip (project.verse);

    const auto asked = project.arrangement.askContext();

    REQUIRE (asked.scope == AskScope::clips);
    REQUIRE (asked.clips == std::vector<ClipRef> { project.verse, project.outro });

    // Two clips are not one thing to name; the panel counts them instead.
    REQUIRE (asked.name.empty());
}

TEST_CASE ("asking about a track makes that track the context")
{
    AskArrangement project;

    project.select ({ project.verse });
    project.arrangement.askAboutTrack (project.keys);

    const auto asked = project.arrangement.askContext();

    REQUIRE (asked.scope == AskScope::track);
    REQUIRE (asked.track == project.keys);
    REQUIRE (asked.name == "Keys");
}

TEST_CASE ("with nothing asked about, the context is the producer's own selection")
{
    AskArrangement project;

    REQUIRE (project.arrangement.askContext().scope == AskScope::nothing);

    project.select ({ project.verse });

    REQUIRE (project.arrangement.askContext().scope == AskScope::clips);
    REQUIRE (project.arrangement.askContext().clips == std::vector<ClipRef> { project.verse });
    REQUIRE (project.arrangement.askContext().name == "Verse");

    project.arrangement.selection().clear();
    project.arrangement.focusTrack (project.drums);

    REQUIRE (project.arrangement.askContext().scope == AskScope::track);
    REQUIRE (project.arrangement.askContext().track == project.drums);
}

TEST_CASE ("an implicit context stands until the producer selects something else")
{
    AskArrangement project;

    project.select ({ project.verse });
    project.arrangement.askAboutClip (project.drop);

    // It is the context of the message the producer is about to send, and of the
    // one after that: nothing but their own hand takes it away.
    REQUIRE (project.arrangement.askContext().clips == std::vector<ClipRef> { project.drop });

    project.select ({ project.outro });

    REQUIRE (project.arrangement.askContext().clips == std::vector<ClipRef> { project.outro });
    REQUIRE (project.arrangement.askContext().name == "Outro");
}

TEST_CASE ("a track asked about is forgotten once the producer's hand moves to another")
{
    AskArrangement project;

    project.arrangement.focusTrack (project.drums);
    project.arrangement.askAboutTrack (project.keys);

    REQUIRE (project.arrangement.askContext().track == project.keys);

    project.arrangement.focusTrack (project.bass);

    REQUIRE (project.arrangement.askContext().track == project.bass);
    REQUIRE (project.arrangement.askContext().name == "Bass");
}

TEST_CASE ("a Task Run's opening context names the clips the producer asked about")
{
    AskArrangement project;

    project.select ({ project.verse });
    project.arrangement.askAboutClip (project.drop);

    const auto opening =
        duet::app::openingContextOf ({ project.arrangement.askContext(), 32.0, 4.0, true });

    REQUIRE (opening.selection == duet::collab::SelectionKind::clips);
    REQUIRE (opening.selectionIds
             == std::vector<std::string> { duet::collab::toolId::forClip (project.drop) });

    // Bars and beats as the producer reads them: both count from one, so the
    // thirty-third beat of a four-four project is bar 9 beat 1.
    REQUIRE (opening.playheadBar == 9);
    REQUIRE (opening.playheadBeat == 1.0);
    REQUIRE (opening.transportPlaying);
}

TEST_CASE ("a Task Run's opening context names the track the producer asked about")
{
    AskArrangement project;

    project.arrangement.askAboutTrack (project.keys);

    const auto opening =
        duet::app::openingContextOf ({ project.arrangement.askContext(), 0.0, 4.0, false });

    REQUIRE (opening.selection == duet::collab::SelectionKind::tracks);
    REQUIRE (opening.selectionIds
             == std::vector<std::string> { duet::collab::toolId::forTrack (project.keys) });
    REQUIRE (opening.playheadBar == 1);
    REQUIRE (opening.playheadBeat == 1.0);
    REQUIRE_FALSE (opening.transportPlaying);
}

TEST_CASE ("a Task Run started with nothing to point at carries no selection")
{
    const AskArrangement project;

    const auto opening =
        duet::app::openingContextOf ({ project.arrangement.askContext(), 0.0, 4.0, false });

    REQUIRE (opening.selection == duet::collab::SelectionKind::none);
    REQUIRE (opening.selectionIds.empty());
}
