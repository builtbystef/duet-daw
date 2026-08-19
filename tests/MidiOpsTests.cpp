#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

using duet::model::ClipRef;
using duet::model::NoteRef;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;

namespace
{
/** A session holding one midi track with one empty four-bar MIDI clip on it. */
struct MidiFixture
{
    TempProject project;
    Session session { project.editFile() };
    TrackRef track = duet::model::noTrack;
    ClipRef clip = duet::model::noClip;

    MidiFixture()
    {
        session.performAction ("Lay out a midi track",
                               [this] (auto& ops)
                               {
                                   track = ops.createTrack (
                                       TrackKind::midi, "Keys", duet::model::BuiltinPlugin::synth);
                                   clip = ops.insertMidiClip (track, "Phrase", 0.0, 8.0);
                               });
    }
};
} // namespace

TEST_CASE ("a MIDI clip goes on to a midi track, ready for notes")
{
    const MidiFixture fixture;

    REQUIRE (fixture.track != duet::model::noTrack);
    REQUIRE (fixture.clip != duet::model::noClip);

    const auto keys = fixture.session.track (fixture.track);

    REQUIRE (keys.kind == TrackKind::midi);
    REQUIRE (keys.clips.size() == 1);
    REQUIRE (keys.clips.front().holdsMidi);
    REQUIRE (fixture.session.notes (fixture.clip).empty());
}

TEST_CASE ("adding a note is one Action, reads back, and undoes and redoes exactly")
{
    MidiFixture fixture;
    auto& session = fixture.session;

    const auto before = session.stateDigest();

    NoteRef note = duet::model::noNote;

    session.performAction (
        "Add note", [&] (auto& ops) { note = ops.addNote (fixture.clip, 60, 1.0, 0.5, 100); });

    const auto after = session.stateDigest();

    REQUIRE (session.undoNames().front() == "Add note");
    REQUIRE (after != before);

    const auto notes = session.notes (fixture.clip);

    REQUIRE (notes.size() == 1);
    REQUIRE (notes.front().note == note);
    REQUIRE (notes.front().pitch == 60);
    REQUIRE (notes.front().startBeats == 1.0);
    REQUIRE (notes.front().lengthBeats == 0.5);
    REQUIRE (notes.front().velocity == 100);

    REQUIRE (session.undo());
    REQUIRE (session.stateDigest() == before);
    REQUIRE (session.notes (fixture.clip).empty());

    REQUIRE (session.redo());
    REQUIRE (session.stateDigest() == after);
    REQUIRE (session.notes (fixture.clip).size() == 1);
}

TEST_CASE ("a note is moved, resized, given a velocity, and removed")
{
    MidiFixture fixture;
    auto& session = fixture.session;

    NoteRef note = duet::model::noNote;

    session.performAction (
        "Add note", [&] (auto& ops) { note = ops.addNote (fixture.clip, 60, 1.0, 0.5, 100); });

    SECTION ("moving takes it to another pitch and another beat")
    {
        session.performAction ("Move the note", [&] (auto& ops) { ops.moveNote (note, 67, 2.5); });

        const auto moved = session.notes (fixture.clip).front();

        REQUIRE (moved.note == note);
        REQUIRE (moved.pitch == 67);
        REQUIRE (moved.startBeats == 2.5);
        REQUIRE (moved.lengthBeats == 0.5);

        REQUIRE (session.undo());
        REQUIRE (session.notes (fixture.clip).front().pitch == 60);
        REQUIRE (session.notes (fixture.clip).front().startBeats == 1.0);
    }

    SECTION ("resizing changes the length and leaves the start alone")
    {
        session.performAction ("Resize the note", [&] (auto& ops) { ops.resizeNote (note, 2.0); });

        REQUIRE (session.notes (fixture.clip).front().lengthBeats == 2.0);
        REQUIRE (session.notes (fixture.clip).front().startBeats == 1.0);

        REQUIRE (session.undo());
        REQUIRE (session.notes (fixture.clip).front().lengthBeats == 0.5);
    }

    SECTION ("a velocity is set on its own")
    {
        session.performAction ("Set the velocity",
                               [&] (auto& ops) { ops.setNoteVelocity (note, 40); });

        REQUIRE (session.notes (fixture.clip).front().velocity == 40);

        REQUIRE (session.undo());
        REQUIRE (session.notes (fixture.clip).front().velocity == 100);
    }

    SECTION ("removing takes it away, and undo brings back the same note")
    {
        session.performAction ("Remove the note", [&] (auto& ops) { ops.removeNote (note); });

        REQUIRE (session.notes (fixture.clip).empty());

        REQUIRE (session.undo());

        const auto restored = session.notes (fixture.clip);

        REQUIRE (restored.size() == 1);

        // The handle still names the note, so an edit made before the undo can
        // be made again after it.
        REQUIRE (restored.front().note == note);
    }
}
