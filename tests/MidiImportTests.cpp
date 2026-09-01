#include <duet/model/MidiImport.h>
#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;
using duet::model::ClipRef;
using duet::model::MidiImportedNote;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;

namespace
{
constexpr double beatsEpsilon = 1e-9;
constexpr double secondsEpsilon = 0.000001;
constexpr double defaultTempoBpm = 120.0;

void appendU16 (std::vector<std::uint8_t>& out, std::uint16_t value)
{
    out.push_back (static_cast<std::uint8_t> (value >> 8));
    out.push_back (static_cast<std::uint8_t> (value & 0xFFU));
}

void appendU32 (std::vector<std::uint8_t>& out, std::uint32_t value)
{
    appendU16 (out, static_cast<std::uint16_t> (value >> 16));
    appendU16 (out, static_cast<std::uint16_t> (value & 0xFFFFU));
}

/** A Standard MIDI File from already-encoded track bodies. Lengths are written
    here so extra cases cannot disagree with the parser about chunk size.
*/
std::vector<std::uint8_t> smf (std::uint16_t format,
                               std::uint16_t division,
                               const std::vector<std::vector<std::uint8_t>>& tracks)
{
    std::vector<std::uint8_t> out { 0x4D, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06 };
    appendU16 (out, format);
    appendU16 (out, static_cast<std::uint16_t> (tracks.size()));
    appendU16 (out, division);

    for (const auto& track : tracks)
    {
        out.insert (out.end(), { 0x4D, 0x54, 0x72, 0x6B });
        appendU32 (out, static_cast<std::uint32_t> (track.size()));
        out.insert (out.end(), track.begin(), track.end());
    }

    return out;
}

/** Format 0, 480 PPQ. One C4 (pitch 60) starts at tick 480 — one quarter note,
    independently one beat — lasts 240 ticks, independently half a beat, at
    velocity 100. Leading silence before the note is kept.
*/
constexpr std::array<std::uint8_t, 36> format0Ppq { 0x4D, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06,
                                                    0x00, 0x00, 0x00, 0x01, 0x01, 0xE0, 0x4D, 0x54,
                                                    0x72, 0x6B, 0x00, 0x00, 0x00, 0x0E, 0x83, 0x60,
                                                    0x90, 0x3C, 0x64, 0x81, 0x70, 0x80, 0x3C, 0x40,
                                                    0x00, 0xFF, 0x2F, 0x00 };

/** Format 1, 480 PPQ, three tracks. Track 0 carries tempo 140 bpm and 4/4, which
    must not become note data. Track 1 holds C4 (60) at tick 0 for 480 ticks at
    velocity 100. Track 2 holds E4 (64) at tick 0 for 480 ticks at velocity 90.
    Independently both notes last one beat and sort C4 then E4.
*/
constexpr std::array<std::uint8_t, 83> format1Ppq {
    0x4D, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x03, 0x01, 0xE0,
    0x4D, 0x54, 0x72, 0x6B, 0x00, 0x00, 0x00, 0x13, 0x00, 0xFF, 0x51, 0x03, 0x06, 0x8A,
    0x3B, 0x00, 0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08, 0x00, 0xFF, 0x2F, 0x00, 0x4D,
    0x54, 0x72, 0x6B, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x90, 0x3C, 0x64, 0x83, 0x60, 0x80,
    0x3C, 0x40, 0x00, 0xFF, 0x2F, 0x00, 0x4D, 0x54, 0x72, 0x6B, 0x00, 0x00, 0x00, 0x0D,
    0x00, 0x90, 0x40, 0x5A, 0x83, 0x60, 0x80, 0x40, 0x40, 0x00, 0xFF, 0x2F, 0x00
};

/** Format 0, SMPTE 25 fps, 40 ticks per frame. 1000 ticks are independently one
    second. One G4 (pitch 67) occupies that second at velocity 80. At 120 bpm a
    second is two beats.
*/
constexpr std::array<std::uint8_t, 35> format0Smpte { 0x4D, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00,
                                                      0x06, 0x00, 0x00, 0x00, 0x01, 0xE7, 0x28,
                                                      0x4D, 0x54, 0x72, 0x6B, 0x00, 0x00, 0x00,
                                                      0x0D, 0x00, 0x90, 0x43, 0x50, 0x87, 0x68,
                                                      0x80, 0x43, 0x40, 0x00, 0xFF, 0x2F, 0x00 };

constexpr std::array<std::uint8_t, 14> format2Header { 0x4D, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00,
                                                       0x06, 0x00, 0x02, 0x00, 0x00, 0x01, 0xE0 };

constexpr std::array<std::uint8_t, 6> truncatedHeader { 0x4D, 0x54, 0x68, 0x64, 0x00, 0x00 };

void requireNote (const MidiImportedNote& note,
                  double startBeats,
                  double lengthBeats,
                  int pitch,
                  int velocity)
{
    REQUIRE_THAT (note.startBeats, WithinAbs (startBeats, beatsEpsilon));
    REQUIRE_THAT (note.lengthBeats, WithinAbs (lengthBeats, beatsEpsilon));
    REQUIRE (note.pitch == pitch);
    REQUIRE (note.velocity == velocity);
}

[[nodiscard]] bool sameImport (const duet::model::MidiImport& first,
                               const duet::model::MidiImport& second)
{
    if (first.ok != second.ok || first.message != second.message
        || first.unmatchedNoteOns != second.unmatchedNoteOns
        || first.unsupportedEvents != second.unsupportedEvents
        || first.notes.size() != second.notes.size())
        return false;

    for (std::size_t index = 0; index < first.notes.size(); ++index)
    {
        const auto& left = first.notes[index];
        const auto& right = second.notes[index];

        if (left.pitch != right.pitch || left.velocity != right.velocity
            || std::abs (left.startBeats - right.startBeats) > beatsEpsilon
            || std::abs (left.lengthBeats - right.lengthBeats) > beatsEpsilon)
            return false;
    }

    return true;
}
} // namespace

TEST_CASE ("a format-0 PPQ file yields independently stated note values", "[midi-import]")
{
    const auto imported = duet::model::parseStandardMidiFile (format0Ppq, defaultTempoBpm);

    REQUIRE (imported.ok);
    REQUIRE (imported.notes.size() == 1);
    requireNote (imported.notes.front(), 1.0, 0.5, 60, 100);
    REQUIRE (imported.unmatchedNoteOns == 0);
    REQUIRE (imported.unsupportedEvents == 0);
    REQUIRE (imported.message == "1 note, 0 unmatched note-ons, 0 unsupported events.");
}

TEST_CASE ("a format-1 PPQ file merges tracks and ignores file tempo", "[midi-import]")
{
    const auto imported = duet::model::parseStandardMidiFile (format1Ppq, 100.0);
    const auto atAnotherTempo = duet::model::parseStandardMidiFile (format1Ppq, 180.0);

    REQUIRE (imported.ok);
    REQUIRE (imported.notes.size() == 2);
    requireNote (imported.notes[0], 0.0, 1.0, 60, 100);
    requireNote (imported.notes[1], 0.0, 1.0, 64, 90);
    REQUIRE (imported.unsupportedEvents == 2);
    REQUIRE (imported.message == "2 notes, 0 unmatched note-ons, 2 unsupported events.");
    REQUIRE (sameImport (imported, atAnotherTempo));
}

TEST_CASE ("an SMPTE file converts source seconds through the project tempo", "[midi-import]")
{
    const auto at120 = duet::model::parseStandardMidiFile (format0Smpte, 120.0);
    const auto at60 = duet::model::parseStandardMidiFile (format0Smpte, 60.0);

    REQUIRE (at120.ok);
    REQUIRE (at120.notes.size() == 1);
    requireNote (at120.notes.front(), 0.0, 2.0, 67, 80);
    REQUIRE (at60.ok);
    requireNote (at60.notes.front(), 0.0, 1.0, 67, 80);
}

TEST_CASE ("overlapping same-pitch notes pair FIFO and unmatched note-offs are ignored",
           "[midi-import]")
{
    const auto overlapping =
        smf (0, 480, { { 0x00, 0x90, 0x3C, 0x64, 0x81, 0x70, 0x90, 0x3C, 0x50, 0x81,
                         0x70, 0x80, 0x3C, 0x40, 0x81, 0x70, 0x80, 0x3C, 0x40, 0x81,
                         0x70, 0x80, 0x3C, 0x40, 0x00, 0xFF, 0x2F, 0x00 } });
    const auto imported = duet::model::parseStandardMidiFile (overlapping, defaultTempoBpm);

    REQUIRE (imported.ok);
    REQUIRE (imported.notes.size() == 2);
    requireNote (imported.notes[0], 0.0, 1.0, 60, 100);
    requireNote (imported.notes[1], 0.5, 1.0, 60, 80);
    REQUIRE (imported.unmatchedNoteOns == 0);
}

TEST_CASE ("coincident notes on two channels remain distinct and sort by channel", "[midi-import]")
{
    const auto coincident =
        smf (0, 480, { { 0x00, 0x90, 0x3C, 0x64, 0x00, 0x91, 0x3C, 0x50, 0x83, 0x60, 0x80,
                         0x3C, 0x40, 0x00, 0x81, 0x3C, 0x40, 0x00, 0xFF, 0x2F, 0x00 } });
    const auto imported = duet::model::parseStandardMidiFile (coincident, defaultTempoBpm);

    REQUIRE (imported.ok);
    REQUIRE (imported.notes.size() == 2);
    requireNote (imported.notes[0], 0.0, 1.0, 60, 100);
    requireNote (imported.notes[1], 0.0, 1.0, 60, 80);
}

TEST_CASE ("same-pitch notes on two format-1 tracks sort by source track", "[midi-import]")
{
    const auto twoTracks =
        smf (1,
             480,
             { { 0x00, 0xFF, 0x2F, 0x00 },
               { 0x00, 0x90, 0x3C, 0x64, 0x83, 0x60, 0x80, 0x3C, 0x40, 0x00, 0xFF, 0x2F, 0x00 },
               { 0x00, 0x90, 0x3C, 0x50, 0x83, 0x60, 0x80, 0x3C, 0x40, 0x00, 0xFF, 0x2F, 0x00 } });
    const auto imported = duet::model::parseStandardMidiFile (twoTracks, defaultTempoBpm);

    REQUIRE (imported.ok);
    REQUIRE (imported.notes.size() == 2);
    requireNote (imported.notes[0], 0.0, 1.0, 60, 100);
    requireNote (imported.notes[1], 0.0, 1.0, 60, 80);
}

TEST_CASE ("unmatched note-ons are counted and a file with none valid is an error", "[midi-import]")
{
    const auto hanging = smf (0, 480, { { 0x00, 0x90, 0x3C, 0x64, 0x00, 0xFF, 0x2F, 0x00 } });
    const auto imported = duet::model::parseStandardMidiFile (hanging, defaultTempoBpm);

    REQUIRE_FALSE (imported.ok);
    REQUIRE (imported.notes.empty());
    REQUIRE (imported.unmatchedNoteOns == 1);
    REQUIRE (imported.message == "This MIDI file has no notes to import.");
}

TEST_CASE ("malformed and empty files produce an actionable result", "[midi-import]")
{
    const auto empty = duet::model::parseStandardMidiFile ({}, defaultTempoBpm);
    const auto truncated = duet::model::parseStandardMidiFile (truncatedHeader, defaultTempoBpm);
    const auto format2 = duet::model::parseStandardMidiFile (format2Header, defaultTempoBpm);
    const auto silent = duet::model::parseStandardMidiFile (
        smf (0, 480, { { 0x00, 0xFF, 0x2F, 0x00 } }), defaultTempoBpm);

    REQUIRE_FALSE (empty.ok);
    REQUIRE (empty.message == "This MIDI file is empty.");
    REQUIRE (empty.notes.empty());

    REQUIRE_FALSE (truncated.ok);
    REQUIRE (truncated.message == "This MIDI file could not be read.");
    REQUIRE (truncated.notes.empty());

    REQUIRE_FALSE (format2.ok);
    REQUIRE (format2.message == "This MIDI file could not be read.");

    REQUIRE_FALSE (silent.ok);
    REQUIRE (silent.message == "This MIDI file has no notes to import.");
    REQUIRE (silent.notes.empty());
}

TEST_CASE ("merge order and summary text are deterministic across runs", "[midi-import]")
{
    const auto first = duet::model::parseStandardMidiFile (format1Ppq, defaultTempoBpm);
    const auto second = duet::model::parseStandardMidiFile (format1Ppq, defaultTempoBpm);

    REQUIRE (first.ok);
    REQUIRE (sameImport (first, second));
    REQUIRE (first.message == "2 notes, 0 unmatched note-ons, 2 unsupported events.");
}

namespace
{
struct MidiImportFixture
{
    TempProject project;
    Session session { project.editFile() };
    TrackRef track = duet::model::noTrack;

    MidiImportFixture()
    {
        session.performAction ("Lay out a midi track",
                               [this] (auto& ops) {
                                   track = ops.createTrack (
                                       TrackKind::midi, "Keys", duet::model::BuiltinPlugin::synth);
                               });
    }
};
} // namespace

TEST_CASE ("Import MIDI inserts one clip of notes and undoes digest-exactly", "[midi-import]")
{
    MidiImportFixture fixture;
    auto& session = fixture.session;
    const auto imported = duet::model::parseStandardMidiFile (format0Ppq, session.tempoBpm());
    REQUIRE (imported.ok);

    const auto before = session.stateDigest();
    ClipRef clip = duet::model::noClip;

    session.performAction (
        "Import MIDI",
        [&] (auto& ops) { clip = ops.importMidi (fixture.track, "Phrase", 4.0, 0.25, imported); });

    REQUIRE (clip != duet::model::noClip);
    REQUIRE (session.undoNames().front() == "Import MIDI");

    const auto clips = session.track (fixture.track).clips;
    REQUIRE (clips.size() == 1);
    REQUIRE (clips.front().holdsMidi);
    REQUIRE (clips.front().sourceReference.empty());
    REQUIRE (clips.front().sourceFile.empty());
    REQUIRE_THAT (clips.front().startSeconds, WithinAbs (2.0, secondsEpsilon));
    REQUIRE_THAT (clips.front().lengthSeconds, WithinAbs (0.75, secondsEpsilon));

    const auto notes = session.notes (clip);
    REQUIRE (notes.size() == 1);
    REQUIRE (notes.front().pitch == 60);
    REQUIRE_THAT (notes.front().startBeats, WithinAbs (1.0, beatsEpsilon));
    REQUIRE_THAT (notes.front().lengthBeats, WithinAbs (0.5, beatsEpsilon));
    REQUIRE (notes.front().velocity == 100);

    const auto after = session.stateDigest();
    REQUIRE (after != before);
    REQUIRE (session.undo());
    REQUIRE (session.stateDigest() == before);
    REQUIRE (session.track (fixture.track).clips.empty());
    REQUIRE (session.redo());
    REQUIRE (session.stateDigest() == after);
    REQUIRE (session.notes (session.track (fixture.track).clips.front().clip).size() == 1);
}

TEST_CASE ("imported tempo and metre do not change the project", "[midi-import]")
{
    MidiImportFixture fixture;
    auto& session = fixture.session;

    session.performAction ("Count it in three",
                           [] (auto& ops)
                           {
                               ops.setTempo (100.0);
                               ops.setTimeSignature (3, 4);
                           });

    const auto imported = duet::model::parseStandardMidiFile (format1Ppq, session.tempoBpm());
    REQUIRE (imported.ok);
    requireNote (imported.notes[0], 0.0, 1.0, 60, 100);

    session.performAction ("Import MIDI",
                           [&] (auto& ops)
                           { ops.importMidi (fixture.track, "Phrase", 0.0, 0.25, imported); });

    REQUIRE_THAT (session.tempoBpm(), WithinAbs (100.0, secondsEpsilon));
    REQUIRE (session.timeSignature().numerator == 3);
    REQUIRE (session.timeSignature().denominator == 4);

    const auto clips = session.track (fixture.track).clips;
    REQUIRE (clips.size() == 1);
    REQUIRE_THAT (clips.front().lengthSeconds, WithinAbs (0.6, secondsEpsilon));
}

TEST_CASE ("clip length is at least the grid subdivision", "[midi-import]")
{
    MidiImportFixture fixture;
    const auto shortNote = smf (
        0, 480, { { 0x00, 0x90, 0x3C, 0x64, 0x30, 0x80, 0x3C, 0x40, 0x00, 0xFF, 0x2F, 0x00 } });
    const auto imported =
        duet::model::parseStandardMidiFile (shortNote, fixture.session.tempoBpm());
    REQUIRE (imported.ok);
    requireNote (imported.notes.front(), 0.0, 0.1, 60, 100);

    fixture.session.performAction (
        "Import MIDI",
        [&] (auto& ops) { ops.importMidi (fixture.track, "Phrase", 0.0, 1.0, imported); });

    const auto clips = fixture.session.track (fixture.track).clips;
    REQUIRE (clips.size() == 1);
    REQUIRE_THAT (clips.front().lengthSeconds, WithinAbs (0.5, secondsEpsilon));
}

TEST_CASE ("malformed files emit no Action", "[midi-import]")
{
    const MidiImportFixture fixture;
    const auto before = fixture.session.stateDigest();
    const auto undoBefore = fixture.session.undoNames();
    const auto imported = duet::model::parseStandardMidiFile ({}, fixture.session.tempoBpm());

    REQUIRE_FALSE (imported.ok);
    REQUIRE_FALSE (imported.message.empty());
    REQUIRE (imported.notes.empty());
    REQUIRE (fixture.session.stateDigest() == before);
    REQUIRE (fixture.session.undoNames() == undoBefore);
    REQUIRE (fixture.session.track (fixture.track).clips.empty());
}
