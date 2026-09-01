#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace duet::model
{
/** A note taken from a Standard MIDI File, in the project's beats.

    Channel and source track are discarded: milestone-one notes have none.
    Coincident notes remain distinct entries.
*/
struct MidiImportedNote
{
    double startBeats = 0.0;
    double lengthBeats = 0.0;
    int pitch = 0;
    int velocity = 0;
};

/** What parsing a Standard MIDI File produced.

    `ok` means there is at least one valid note to materialize. `message` is
    the producer-facing summary when it is ok, and the producer-facing reason
    when it is not.
*/
struct MidiImport
{
    bool ok = false;
    std::string message;
    std::vector<MidiImportedNote> notes;
    int unmatchedNoteOns = 0;
    int unsupportedEvents = 0;
};

/** Parses a Standard MIDI File from its bytes.

    Accepts SMF format 0 and 1. PPQ files map one quarter note to one project
    beat. SMPTE files convert source seconds to beats using `projectTempoBpm`.
    Tempo, metre, and key events in the file never become project data.
*/
[[nodiscard]] MidiImport parseStandardMidiFile (std::span<const std::uint8_t> bytes,
                                                double projectTempoBpm);
} // namespace duet::model
