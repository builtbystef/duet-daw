#include <duet/gui/ArrangementView.h>

#include <duet/gui/ViewState.h>

#include <duet/model/Session.h>
#include <duet/persistence/Project.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

using duet::gui::ArrangementView;
using duet::gui::ViewState;
using duet::model::Session;
using duet::model::TrackColour;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;

namespace
{
struct TrackArrangement
{
    TrackArrangement() : session (project.editFile())
    {
        arrangement.setSession (&session);
        arrangement.setWidthPx (800);
        arrangement.setHeightPx (300);
        view.setHZoomPxPerBeat (40.0);
    }

    TempProject project;
    Session session;
    ViewState view;
    ArrangementView arrangement { view };
};

[[nodiscard]] std::vector<std::string> trackNames (const Session& session)
{
    std::vector<std::string> names;

    for (const auto& track : session.tracks())
        names.push_back (track.name);

    return names;
}
} // namespace

TEST_CASE ("track lanes map clips through the timeline geometry and carry the track colour")
{
    TrackArrangement open;
    TrackRef bass = duet::model::noTrack;
    duet::model::ClipRef phrase = duet::model::noClip;

    open.session.performAction ("Build Bass",
                                [&] (auto& ops)
                                {
                                    bass = ops.createTrack (TrackKind::midi, "Bass");
                                    ops.setTrackColour (bass, TrackColour::mint);
                                    phrase = ops.insertMidiClip (bass, "Phrase", 1.0, 2.0);
                                    ops.addNote (phrase, 48, 0.5, 1.0, 100);
                                });

    const auto rows = open.arrangement.tracks();
    const auto row =
        std::find_if (rows.begin(),
                      rows.end(),
                      [bass] (const auto& candidate) { return candidate.track == bass; });

    REQUIRE (row != rows.end());
    REQUIRE (row->colour == TrackColour::mint);
    REQUIRE (row->clips.size() == 1);
    REQUIRE (row->clips.front().x == open.arrangement.geometry().beatsToX (2.0));
    REQUIRE (row->clips.front().width
             == open.arrangement.geometry().beatsToX (6.0)
                    - open.arrangement.geometry().beatsToX (2.0));
    REQUIRE (row->clips.front().notes.size() == 1);
}

TEST_CASE ("the add-track row emits one named Action and follows undo and redo")
{
    TrackArrangement open;
    const auto before = open.session.tracks().size();

    const auto audio = open.arrangement.addTrack (TrackKind::audio);
    REQUIRE (audio != duet::model::noTrack);
    REQUIRE (open.session.tracks().size() == before + 1);
    REQUIRE (open.session.undoNames().front() == "Add Audio Track");
    REQUIRE (open.view.hasTrack (audio));

    REQUIRE (open.session.undo());
    REQUIRE (open.session.tracks().size() == before);
    REQUIRE (open.session.redo());
    REQUIRE (open.session.tracks().size() == before + 1);
}

TEST_CASE ("dragging C above B is one Action and undo restores A B C")
{
    TrackArrangement open;
    TrackRef c = duet::model::noTrack;
    open.session.performAction ("Add A B C",
                                [&] (auto& ops)
                                {
                                    ops.createTrack (TrackKind::audio, "A");
                                    ops.createTrack (TrackKind::audio, "B");
                                    c = ops.createTrack (TrackKind::audio, "C");
                                });

    const auto before = trackNames (open.session);
    const auto cIndex = static_cast<int> (before.size()) - 1;
    open.arrangement.reorderTrack (c, cIndex - 1);

    auto reordered = before;
    std::iter_swap (reordered.end() - 1, reordered.end() - 2);
    REQUIRE (trackNames (open.session) == reordered);
    REQUIRE (open.session.undoNames().front() == "Reorder Track");
    REQUIRE (open.session.undo());
    REQUIRE (trackNames (open.session) == before);
}

TEST_CASE ("track header edits each cross the Action seam once")
{
    TrackArrangement open;
    const auto track = open.arrangement.addTrack (TrackKind::midi);

    open.arrangement.renameTrack (track, "Keys");
    REQUIRE (open.session.track (track).name == "Keys");
    REQUIRE (open.session.undoNames().front() == "Rename Track");

    open.arrangement.cyclePan (track);
    REQUIRE (open.session.track (track).pan == 1.0);
    REQUIRE (open.session.undoNames().front() == "Pan Track");

    open.arrangement.toggleMute (track);
    REQUIRE (open.session.track (track).muted);
    REQUIRE (open.session.undoNames().front() == "Mute Track");

    open.arrangement.toggleSolo (track);
    REQUIRE (open.session.track (track).soloed);
    REQUIRE (open.session.undoNames().front() == "Solo Track");

    open.arrangement.setTrackColour (track, TrackColour::blue);
    REQUIRE (open.session.track (track).colour == TrackColour::blue);
    REQUIRE (open.session.undoNames().front() == "Set Track Colour");
}

TEST_CASE ("the record-arm header toggle is reflected in the model without becoming an Action")
{
    TrackArrangement open;
    open.session.useNoAudioDevice();
    const auto track = open.arrangement.addTrack (TrackKind::audio);
    const auto inputs = open.session.availableInputs();
    const auto input = std::find_if (inputs.begin(),
                                     inputs.end(),
                                     [] (const auto& candidate)
                                     { return candidate.kind == duet::model::InputKind::audio; });
    REQUIRE (input != inputs.end());

    open.session.setTrackInput (track, input->input);
    const auto undoBefore = open.session.undoNames();
    open.arrangement.toggleRecordArm (track);

    REQUIRE (open.session.track (track).recordArmed);
    REQUIRE (open.session.undoNames() == undoBefore);
}

TEST_CASE ("duplicate and delete are digest-exact Actions and clean up project view rows")
{
    TrackArrangement open;
    const auto source = open.arrangement.addTrack (TrackKind::midi);
    const auto beforeDuplicate = open.session.stateDigest();

    const auto copy = open.arrangement.duplicateTrack (source);
    REQUIRE (open.view.hasTrack (copy));
    REQUIRE (open.session.undoNames().front() == "Duplicate Track");
    REQUIRE (open.session.undo());
    REQUIRE (open.session.stateDigest() == beforeDuplicate);
    REQUIRE (open.session.redo());

    const auto beforeDelete = open.session.stateDigest();
    open.arrangement.deleteTrack (copy);
    REQUIRE_FALSE (open.view.hasTrack (copy));
    REQUIRE (open.session.undoNames().front() == "Delete Track");
    REQUIRE (open.session.undo());
    REQUIRE (open.session.stateDigest() == beforeDelete);
}

TEST_CASE ("a track colour survives save and reopen")
{
    const TempProject temp;
    const auto folder = temp.folder() / "Colours";
    auto project = duet::persistence::Project::create (folder);
    REQUIRE (project != nullptr);

    TrackRef track = duet::model::noTrack;
    project->session().performAction ("Colour Bass",
                                      [&] (auto& ops)
                                      {
                                          track = ops.createTrack (TrackKind::audio, "Bass");
                                          ops.setTrackColour (track, TrackColour::coral);
                                      });
    REQUIRE (project->save());
    project.reset();

    const auto reopened = duet::persistence::Project::open (folder);
    REQUIRE (reopened != nullptr);
    REQUIRE (reopened->session().track (track).colour == TrackColour::coral);
}

TEST_CASE ("track height belongs only to VIEW and vertical scrolling stops at its content")
{
    TrackArrangement open;
    const auto first = open.arrangement.addTrack (TrackKind::audio);
    const auto second = open.arrangement.addTrack (TrackKind::audio);
    const auto undoBefore = open.session.undoNames();
    const auto digestBefore = open.session.stateDigest();

    open.arrangement.resizeTrack (first, 120);

    REQUIRE (open.view.trackHeightPx (first) == 120);
    REQUIRE (open.view.trackHeightPx (second) == ViewState::defaultTrackHeightPx);
    REQUIRE (open.session.undoNames() == undoBefore);
    REQUIRE (open.session.stateDigest() == digestBefore);

    ViewState reopened;
    reopened.readFrom (open.view.toData());
    REQUIRE (reopened.trackHeightPx (first) == 120);

    open.arrangement.setHeightPx (80);
    duet::gui::ScrollGesture down;
    down.deltaY = -100.0;
    open.arrangement.scroll (down);
    REQUIRE (open.view.vScrollPx() == open.arrangement.contentHeightPx() - 80);
}
