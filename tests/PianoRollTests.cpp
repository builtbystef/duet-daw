#include <duet/gui/PianoRoll.h>
#include <duet/gui/ViewState.h>

#include <duet/model/Session.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using duet::gui::GridSize;
using duet::gui::NoteGestureKind;
using duet::gui::PianoRoll;
using duet::gui::Scale;
using duet::gui::Selection;
using duet::gui::ViewState;
using duet::model::NoteRef;
using duet::model::Session;
using duet::testing::TempProject;

namespace
{
struct OpenRoll
{
    OpenRoll() : session (project.editFile()), roll (view, selection)
    {
        session.performAction ("Phrase",
                               [&] (auto& ops)
                               {
                                   track = ops.createTrack (duet::model::TrackKind::midi,
                                                            "Keys",
                                                            duet::model::BuiltinPlugin::synth);
                                   clip = ops.insertMidiClip (track, "Verse melody", 0.0, 8.0);
                               });
        roll.setSession (&session);
        roll.openClip (clip);
        view.setGridSize (GridSize::quarter);
    }

    NoteRef add (int pitch, double start, double length, int velocity)
    {
        NoteRef note = duet::model::noNote;
        session.performAction (
            "Seed", [&] (auto& ops) { note = ops.addNote (clip, pitch, start, length, velocity); });
        return note;
    }

    [[nodiscard]] duet::model::NoteInfo note (NoteRef wanted) const
    {
        for (const auto& note : session.notes (clip))
            if (note.note == wanted)
                return note;
        return {};
    }

    TempProject project;
    Session session;
    ViewState view;
    Selection selection;
    PianoRoll roll;
    duet::model::TrackRef track = duet::model::noTrack;
    duet::model::ClipRef clip = duet::model::noClip;
};
} // namespace

TEST_CASE ("a MIDI clip opens as a named piano roll even when it is empty")
{
    const OpenRoll open;
    REQUIRE (open.roll.isOpen());
    REQUIRE (open.roll.clipName() == "Verse melody");
    REQUIRE (open.roll.notes().empty());
    REQUIRE (open.roll.rows().size() == 128);
}

TEST_CASE ("the quarter-note smart tool adds and removes one note per Action")
{
    OpenRoll open;
    open.roll.setNewNoteLengthBeats (1.0);
    const auto before = open.session.undoNames().size();
    const auto note = open.roll.addNote (60, 2.3);
    const auto added = open.note (note);
    REQUIRE (added.pitch == 60);
    REQUIRE_THAT (added.startBeats, WithinAbs (2.0, 1e-9));
    REQUIRE_THAT (added.lengthBeats, WithinAbs (1.0, 1e-9));
    REQUIRE (added.velocity == PianoRoll::defaultVelocity);
    REQUIRE (open.session.undoNames().size() == before + 1);
    open.roll.removeNote (note);
    REQUIRE (open.session.notes (open.clip).empty());
    REQUIRE (open.session.undoNames().front() == "Remove Note");
}

TEST_CASE ("a note drag moves pitch and snapped time in one Action with Alt bypass")
{
    OpenRoll open;
    const auto note = open.add (60, 1.0, 1.0, 100);
    open.view.setGridSize (GridSize::eighth);
    open.roll.beginNoteGesture (note, NoteGestureKind::move, 1.0, 60);
    open.roll.updateNoteGesture (1.62, 64, false);
    REQUIRE (open.roll.completeNoteGesture());
    REQUIRE_THAT (open.note (note).startBeats, WithinAbs (1.5, 1e-9));
    REQUIRE (open.note (note).pitch == 64);
    REQUIRE (open.session.undoNames().front() == "Move Note");

    open.roll.beginNoteGesture (note, NoteGestureKind::move, 1.5, 64);
    open.roll.updateNoteGesture (2.37, 65, true);
    REQUIRE (open.roll.completeNoteGesture());
    REQUIRE_THAT (open.note (note).startBeats, WithinAbs (2.37, 1e-9));
}

TEST_CASE ("a drag keeps hold of where in the note it grabbed")
{
    OpenRoll open;
    const auto note = open.add (60, 2.0, 2.0, 100);

    // Grabbed near its middle and carried 1.4 beats to the right: the note goes
    // where the drag carried it — one whole cell — and not to the pointer's own
    // cell, which by then is two cells over.
    open.roll.beginNoteGesture (note, NoteGestureKind::move, 2.9, 60);
    open.roll.updateNoteGesture (4.3, 58, false);

    SECTION ("the drawing follows the drag before anything is committed")
    {
        const auto drawings = open.roll.notes();
        REQUIRE (drawings.size() == 1);
        REQUIRE_THAT (drawings.front().startBeats, WithinAbs (3.0, 1e-9));
        REQUIRE (drawings.front().pitch == 58);
        REQUIRE_THAT (drawings.front().lengthBeats, WithinAbs (2.0, 1e-9));
    }

    SECTION ("letting go lands the note exactly where the drawing stood")
    {
        REQUIRE (open.roll.completeNoteGesture());
        REQUIRE_THAT (open.note (note).startBeats, WithinAbs (3.0, 1e-9));
        REQUIRE (open.note (note).pitch == 58);
    }
}

TEST_CASE ("a move carries the whole selection in one Action")
{
    OpenRoll open;
    const auto low = open.add (60, 0.0, 1.0, 100);
    const auto high = open.add (64, 2.0, 1.0, 100);
    open.roll.selectNotes ({ low, high }, false);

    open.roll.beginNoteGesture (low, NoteGestureKind::move, 0.2, 60);
    open.roll.updateNoteGesture (1.2, 62, false);

    SECTION ("both drawings move together before the drop")
    {
        for (const auto& drawing : open.roll.notes())
        {
            if (drawing.note == low)
            {
                REQUIRE_THAT (drawing.startBeats, WithinAbs (1.0, 1e-9));
                REQUIRE (drawing.pitch == 62);
            }
            else
            {
                REQUIRE_THAT (drawing.startBeats, WithinAbs (3.0, 1e-9));
                REQUIRE (drawing.pitch == 66);
            }
        }
    }

    SECTION ("the drop is one undoable Action over both notes")
    {
        const auto before = open.session.undoNames().size();
        REQUIRE (open.roll.completeNoteGesture());
        REQUIRE (open.session.undoNames().size() == before + 1);
        REQUIRE (open.session.undoNames().front() == "Move Notes");
        REQUIRE_THAT (open.note (low).startBeats, WithinAbs (1.0, 1e-9));
        REQUIRE (open.note (low).pitch == 62);
        REQUIRE_THAT (open.note (high).startBeats, WithinAbs (3.0, 1e-9));
        REQUIRE (open.note (high).pitch == 66);
    }
}

TEST_CASE ("a note right edge stays positive and resizes in one Action")
{
    OpenRoll open;
    const auto note = open.add (60, 1.0, 1.0, 100);
    open.roll.beginNoteGesture (note, NoteGestureKind::resizeRight, 2.0, 60);
    open.roll.updateNoteGesture (2.6, 60, false);

    // The pulled edge is visible where it will land before the mouse is let go.
    REQUIRE_THAT (open.roll.notes().front().lengthBeats, WithinAbs (2.0, 1e-9));

    REQUIRE (open.roll.completeNoteGesture());
    REQUIRE (open.note (note).lengthBeats == 2.0);
    REQUIRE (open.session.undoNames().front() == "Resize Note");

    open.roll.beginNoteGesture (note, NoteGestureKind::resizeRight, 3.0, 60);
    open.roll.updateNoteGesture (-100.0, 60, false);
    REQUIRE (open.roll.completeNoteGesture());
    REQUIRE (open.note (note).lengthBeats > 0.0);
}

TEST_CASE ("a new note lands in the grid cell the click is in, never the next one")
{
    OpenRoll open;
    open.roll.setNewNoteLengthBeats (1.0);

    // Past the cell's midpoint, still inside the cell: rounding to the nearest
    // line would put the note one cell late.
    const auto note = open.roll.addNote (60, 2.7);

    REQUIRE_THAT (open.note (note).startBeats, WithinAbs (2.0, 1e-9));
}

TEST_CASE ("velocity bars scale the selected notes together and clamp MIDI velocity")
{
    OpenRoll open;
    const auto a = open.add (60, 0.0, 1.0, 100);
    const auto b = open.add (64, 1.0, 1.0, 80);
    const auto c = open.add (67, 2.0, 1.0, 60);
    open.roll.selectNotes ({ a, b, c }, false);
    open.roll.setSelectedVelocity (a, 120);
    REQUIRE (open.note (a).velocity == 120);
    REQUIRE (open.note (b).velocity == 96);
    REQUIRE (open.note (c).velocity == 72);
    REQUIRE (open.session.undoNames().front() == "Scale Note Velocities");
}

TEST_CASE ("piano-roll selection owns only notes and deletes them in one Action")
{
    OpenRoll open;
    open.add (60, 0.0, 1.0, 100);
    open.add (64, 1.0, 1.0, 80);
    open.roll.selectAll();
    REQUIRE (open.selection.items().size() == 2);
    open.roll.deleteSelected();
    REQUIRE (open.session.notes (open.clip).empty());
    REQUIRE (open.session.undoNames().front() == "Delete Notes");
    open.roll.clearSelection();
    REQUIRE (open.selection.empty());
}

TEST_CASE ("scale highlighting and Fold change rows without changing notes")
{
    OpenRoll open;
    open.add (60, 0.0, 1.0, 100);
    open.add (64, 1.0, 1.0, 80);
    open.add (67, 2.0, 1.0, 60);
    const auto digest = open.session.stateDigest();
    open.roll.setScale (0, Scale::major);
    REQUIRE (open.roll.rowForPitch (60).inScale);
    REQUIRE_FALSE (open.roll.rowForPitch (61).inScale);
    REQUIRE (open.session.stateDigest() == digest);
    open.roll.setFolded (true);
    const auto rows = open.roll.rows();
    REQUIRE (rows.size() == 3);
    REQUIRE (rows[0].pitch == 67);
    REQUIRE (rows[1].pitch == 64);
    REQUIRE (rows[2].pitch == 60);
    open.roll.setFolded (false);
    REQUIRE (open.roll.rows().size() == 128);
}

TEST_CASE ("quantize moves a note selection as one undoable Action")
{
    OpenRoll open;
    const auto a = open.add (60, 1.03, 1.0, 100);
    const auto b = open.add (64, 1.62, 1.0, 80);
    open.roll.selectNotes ({ a, b }, false);
    const auto before = open.session.stateDigest();
    open.view.setGridSize (GridSize::eighth);
    open.roll.quantizeSelected();
    REQUIRE_THAT (open.note (a).startBeats, WithinAbs (1.0, 1e-9));
    REQUIRE_THAT (open.note (b).startBeats, WithinAbs (1.5, 1e-9));
    REQUIRE (open.session.undoNames().front() == "Quantize Notes");
    REQUIRE (open.session.undo());
    REQUIRE (open.session.stateDigest() == before);
}

TEST_CASE ("a note added while transport rolls is audible without stopping it")
{
    OpenRoll open;
    open.session.useNoAudioDevice();
    open.session.startPlayback();
    (void) open.roll.addNote (60, 0.0);
    REQUIRE (open.session.isPlaying());
    open.session.runWithoutAudioDevice (0.1);
    REQUIRE (open.session.outputPeakDb() > -80.0);
    open.session.stopPlayback();
}

TEST_CASE ("piano-roll zoom uses shared horizontal view and persisted key height")
{
    OpenRoll open;
    open.view.setHZoomPxPerBeat (80.0);
    open.roll.setKeyHeightPx (18);
    open.roll.scrollVertically (120);
    const auto stored = open.view.toData();
    ViewState reopened;
    reopened.readFrom (stored);
    REQUIRE_THAT (reopened.hZoomPxPerBeat(), WithinAbs (80.0, 1e-9));
    REQUIRE (reopened.pianoRollKeyHeightPx() == 18);
    REQUIRE (reopened.pianoRollVScrollPx() == open.view.pianoRollVScrollPx());
}

TEST_CASE ("the roll ends exactly at its octaves once it knows its height")
{
    OpenRoll open;
    open.roll.setKeyHeightPx (10);
    open.roll.setHeightPx (400);

    SECTION ("scrolling down stops where the lowest pitch meets the bottom edge")
    {
        open.roll.scrollVertically (1000000);

        const auto rows = open.roll.rows();
        REQUIRE (rows.back().pitch == PianoRoll::minimumPitch);
        REQUIRE (rows.back().y + rows.back().height == 400);
    }

    SECTION ("scrolling up stops where the highest pitch meets the top edge")
    {
        open.roll.scrollVertically (-1000000);

        const auto rows = open.roll.rows();
        REQUIRE (rows.front().pitch == PianoRoll::maximumPitch);
        REQUIRE (rows.front().y == 0);
    }

    SECTION ("zooming out pulls a scroll that pointed past the last octave back in")
    {
        open.roll.scrollVertically (1000000);
        open.roll.verticalZoom (0.6);

        const auto rows = open.roll.rows();
        REQUIRE (rows.back().y + rows.back().height == 400);
    }

    SECTION ("a roll shorter than its window starts at the top and has nowhere to scroll")
    {
        open.roll.setHeightPx (2000);
        open.roll.scrollVertically (500);

        REQUIRE (open.roll.rows().front().y == 0);
    }
}
