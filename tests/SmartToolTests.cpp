#include <duet/gui/ArrangementView.h>
#include <duet/gui/Selection.h>
#include <duet/gui/Snap.h>
#include <duet/gui/ViewState.h>

#include <duet/model/Session.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using duet::gui::ArrangementView;
using duet::gui::ClipGestureKind;
using duet::gui::SelectedItem;
using duet::gui::Selection;
using duet::gui::SelectionKind;
using duet::gui::ViewState;
using duet::model::ClipRef;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;

namespace
{
struct SmartArrangement
{
    SmartArrangement() : session (project.editFile())
    {
        arrangement.setSession (&session);
        arrangement.setWidthPx (800);
        view.setHZoomPxPerBeat (20.0);
        session.performAction ("Clips",
                               [&] (auto& ops)
                               {
                                   firstTrack = ops.createTrack (TrackKind::midi, "First");
                                   secondTrack = ops.createTrack (TrackKind::midi, "Second");
                                   for (int index = 0; index < 4; ++index)
                                       clips.push_back (ops.insertMidiClip (
                                           firstTrack,
                                           std::string (1, static_cast<char> ('A' + index)),
                                           index * 2.0,
                                           1.0));
                               });
    }

    [[nodiscard]] duet::model::ClipInfo clip (ClipRef wanted) const
    {
        for (const auto& track : session.tracks())
            for (const auto& candidate : track.clips)
                if (candidate.clip == wanted)
                    return candidate;
        return {};
    }

    TempProject project;
    Session session;
    ViewState view;
    ArrangementView arrangement { view };
    TrackRef firstTrack = duet::model::noTrack;
    TrackRef secondTrack = duet::model::noTrack;
    std::vector<ClipRef> clips;
};

std::vector<SelectedItem> clipItems (const std::vector<ClipRef>& clips)
{
    std::vector<SelectedItem> items;
    items.reserve (clips.size());
    for (const auto clip : clips)
        items.push_back ({ SelectionKind::clip, clip });
    return items;
}
} // namespace

TEST_CASE ("one selection follows click toggle range focus and clear")
{
    Selection selection;
    const auto ordered = clipItems ({ 11, 12, 13, 14 });

    selection.click (ordered[0], ordered, false, false);
    REQUIRE (selection.items() == clipItems ({ 11 }));
    selection.click (ordered[1], ordered, true, false);
    REQUIRE (selection.items() == clipItems ({ 11, 12 }));
    selection.click (ordered[0], ordered, true, false);
    REQUIRE (selection.items() == clipItems ({ 12 }));
    selection.click (ordered[3], ordered, false, true);
    REQUIRE (selection.items() == clipItems ({ 12, 13, 14 }));

    selection.focus (SelectionKind::note);
    selection.selectAll ({ { SelectionKind::note, 21 }, { SelectionKind::note, 22 } });
    REQUIRE (
        selection.items()
        == std::vector<SelectedItem> { { SelectionKind::note, 21 }, { SelectionKind::note, 22 } });
    selection.clear();
    REQUIRE (selection.empty());
}

TEST_CASE ("rubber band replaces or extends the current selection")
{
    Selection selection;
    selection.click ({ SelectionKind::clip, 1 }, clipItems ({ 1, 2, 3 }), false, false);
    selection.rubberBand (clipItems ({ 2, 3 }), false);
    REQUIRE (selection.items() == clipItems ({ 2, 3 }));
    selection.rubberBand (clipItems ({ 1 }), true);
    REQUIRE (selection.items() == clipItems ({ 1, 2, 3 }));
}

TEST_CASE ("arrangement rubber band selects every intersected clip")
{
    SmartArrangement open;
    open.arrangement.selection().click (
        { SelectionKind::clip, open.clips[0] }, open.arrangement.allClipItems(), false, false);

    const auto rows = open.arrangement.tracks();
    const auto row = std::find_if (rows.begin(),
                                   rows.end(),
                                   [&open] (const auto& candidate)
                                   { return candidate.track == open.firstTrack; });
    REQUIRE (row != rows.end());
    open.arrangement.rubberBand ({ 75, row->y, 10, 40 }, false);
    REQUIRE (open.arrangement.selection().items() == clipItems ({ open.clips[1] }));
    open.arrangement.rubberBand ({ 0, row->y, 10, 40 }, true);
    REQUIRE (open.arrangement.selection().items() == clipItems ({ open.clips[0], open.clips[1] }));
}

TEST_CASE ("snap follows the currently held Alt modifier")
{
    const duet::gui::GridSpec grid { 1.0, 4.0 };
    REQUIRE (duet::gui::snapBeats (3.30, grid, false) == 3.0);
    REQUIRE (duet::gui::snapBeats (3.30, grid, true) == 3.30);
    REQUIRE (duet::gui::snapBeats (3.30, grid, false) == 3.0);
}

TEST_CASE ("the transport's eighth-note grid makes a 3.30 beat drag land at 3.5")
{
    SmartArrangement open;
    open.view.setGridSize (duet::gui::GridSize::eighth);

    open.arrangement.beginClipGesture (open.clips[0], ClipGestureKind::move);
    open.arrangement.updateClipGesture (3.30, open.firstTrack, false, false);
    REQUIRE (open.arrangement.completeClipGesture());
    REQUIRE (open.clip (open.clips[0]).startSeconds == 1.75);
}

TEST_CASE ("move copy cross-track drop and Escape cross the Action seam correctly")
{
    SmartArrangement open;
    const auto a = open.clips[0];
    const auto before = open.session.undoNames().size();

    open.arrangement.beginClipGesture (a, ClipGestureKind::move);
    open.arrangement.updateClipGesture (3.30, open.firstTrack, false, false);
    REQUIRE (open.arrangement.completeClipGesture());
    REQUIRE (open.session.undoNames().size() == before + 1);
    REQUIRE (open.session.undoNames().front() == "Move Clip");
    REQUIRE (open.clip (a).startSeconds == 1.5); // 3 beats at 120 BPM

    open.arrangement.beginClipGesture (a, ClipGestureKind::move);
    open.arrangement.updateClipGesture (5.30, open.secondTrack, true, true);
    REQUIRE (open.arrangement.completeClipGesture());
    REQUIRE (open.session.undoNames().front() == "Copy Clip");
    REQUIRE (open.clip (a).startSeconds == 1.5);
    REQUIRE (open.session.track (open.secondTrack).clips.size() == 1);
    const auto copy = open.session.track (open.secondTrack).clips.front().clip;
    REQUIRE (open.arrangement.selection().items() == clipItems ({ copy }));

    open.arrangement.beginClipGesture (copy, ClipGestureKind::move);
    open.arrangement.updateClipGesture (6.0, open.firstTrack, false, false);
    REQUIRE (open.arrangement.completeClipGesture());
    REQUIRE (open.session.track (open.secondTrack).clips.empty());
    REQUIRE (open.clip (copy).startSeconds == 3.0);

    const auto digest = open.session.stateDigest();
    const auto actions = open.session.undoNames();
    open.arrangement.beginClipGesture (copy, ClipGestureKind::move);
    open.arrangement.updateClipGesture (8.0, duet::model::noTrack, false, false);
    REQUIRE_FALSE (open.arrangement.completeClipGesture());
    REQUIRE (open.session.stateDigest() == digest);
    open.arrangement.beginClipGesture (copy, ClipGestureKind::move);
    open.arrangement.updateClipGesture (8.0, open.firstTrack, false, false);
    open.arrangement.cancelClipGesture();
    REQUIRE (open.session.stateDigest() == digest);
    REQUIRE (open.session.undoNames() == actions);
}

TEST_CASE ("trim loop delete and empty-lane creation are one exact Action each")
{
    SmartArrangement open;
    const auto clip = open.clips[1];

    open.arrangement.beginClipGesture (clip, ClipGestureKind::trimLeft);
    open.arrangement.updateClipGesture (5.0, open.firstTrack, false, false);
    REQUIRE (open.arrangement.completeClipGesture());
    REQUIRE (open.session.undoNames().front() == "Trim Clip");
    REQUIRE (open.clip (clip).startSeconds == 2.5);
    REQUIRE (open.clip (clip).lengthSeconds == 0.5);
    REQUIRE (open.clip (clip).contentOffsetSeconds == 0.5);

    const auto beforeLoop = open.session.stateDigest();
    open.arrangement.beginClipGesture (clip, ClipGestureKind::loop);
    open.arrangement.updateClipGesture (7.4, open.firstTrack, false, false);
    REQUIRE (open.arrangement.completeClipGesture());
    REQUIRE (open.session.undoNames().front() == "Loop Clip");
    REQUIRE (open.clip (clip).looped);
    REQUIRE (open.session.undo());
    REQUIRE (open.session.stateDigest() == beforeLoop);

    open.arrangement.selection().rubberBand (clipItems ({ open.clips[0], open.clips[2] }), false);
    const auto beforeDelete = open.session.stateDigest();
    open.arrangement.deleteSelected();
    REQUIRE (open.session.undoNames().front() == "Delete Clips");
    REQUIRE (open.session.undo());
    REQUIRE (open.session.stateDigest() == beforeDelete);

    const auto created = open.arrangement.createMidiClip (open.secondTrack, 5.4);
    REQUIRE (created != duet::model::noClip);
    const auto info = open.clip (created);
    REQUIRE (info.startSeconds == 2.5);
    REQUIRE (info.lengthSeconds == 0.5);
    REQUIRE (open.session.undoNames().front() == "Create MIDI Clip");
}

TEST_CASE ("clipboard Actions paste across tracks and direct routes share them")
{
    SmartArrangement open;
    const auto source = open.clips[0];
    open.arrangement.selection().click (
        { SelectionKind::clip, source }, clipItems (open.clips), false, false);

    open.arrangement.copySelected();
    const auto pasted = open.arrangement.paste (10.0, open.secondTrack);
    REQUIRE (pasted.size() == 1);
    REQUIRE (open.session.undoNames().front() == "Paste Clips");
    REQUIRE (open.session.track (open.secondTrack).clips.front().startSeconds == 5.0);

    open.arrangement.duplicateSelected();
    REQUIRE (open.session.undoNames().front() == "Duplicate Clip");
    open.arrangement.renameSelectedClip ("Renamed");
    REQUIRE (open.session.undoNames().front() == "Rename Clip");
    const auto duplicate = open.arrangement.selection().items().front().ref;
    REQUIRE (open.clip (duplicate).name == "Renamed");

    open.arrangement.setSelectedClipColour (duet::model::TrackColour::blue);
    REQUIRE (open.clip (duplicate).colour == duet::model::TrackColour::blue);

    open.arrangement.cutSelected();
    REQUIRE (open.session.undoNames().front() == "Cut Clip");
    REQUIRE (open.clip (duplicate).clip == duet::model::noClip);
    REQUIRE (open.arrangement.paste (14.0, open.firstTrack).size() == 1);
}
