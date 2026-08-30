#pragma once

#include <duet/collab/Analysis.h>

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

/** The last of the analysis layer's tier-3 routines: the notes a stretch of
    audio holds, and what instrument it sounds like (spec js437t).

    The one place in Duet where a value comes out of a trained model rather than
    out of arithmetic over the signal, and the spec makes it separable on
    purpose: the whole of it is behind one build option, and with that option
    off everything here is still declared, still links, and answers that it has
    nothing to say. Nothing else in the Collaborator notices, and key and chords
    keep working, because they are arithmetic and never needed this.

    Like the harmony routines, and for the same reason, what comes out is a
    guess: notes read out of a waveform are not a property of it. So the notes
    carry a confidence of their own, the instrument carries one beside its name,
    and both cross the seam wrapped and never bare (ADR 0002).

    Nothing here knows about the project, the socket, a track or a thread. It is
    called on the Collaborator service's own thread, takes seconds, and asks
    `keepGoing` between the windows it works through so that a producer who
    cancels is not waited on.
*/
namespace duet::collab::transcription
{
/** One note read out of a waveform: which one it is, where it starts and how
    long it lasts in seconds from that waveform's own start, and how strongly it
    was played, from 0 to 1.

    Pitch is MIDI's: 60 is middle C, and 21 and 108 are the ends of a piano,
    which are also the ends of what the model can name.
*/
struct Note
{
    int pitch = 0;
    double startSeconds = 0.0;
    double lengthSeconds = 0.0;
    double strength = 0.0;
};

/** What was read out of one waveform: the notes, in the order they start, and
    how well the model says it heard them, from 0 to 1.

    The confidence is one number for the whole reading, because the reading is
    one estimate: what it says is how strongly the notes it named were sounding,
    on average.
*/
struct Transcribed
{
    std::vector<Note> notes;
    double confidence = 0.0;
};

/** How the two routines below describe themselves, which is what the `method`
    of a wrapped value says.
*/
inline constexpr std::string_view notesMethod =
    "polyphonic transcription by the Basic Pitch neural model, ICASSP 2022 weights";
inline constexpr std::string_view instrumentMethod =
    "register and polyphony of the transcribed notes, weighted by how tonal the spectrum is";

/** Whether this build can transcribe at all.

    False when the ML runtime was left out at build time, and false when what it
    needs is not on this machine — the model file above all. Both are the same
    fact to a caller: there is no answer to be had, and an aspect with no answer
    is left out of a tool result rather than answered emptily.
*/
[[nodiscard]] bool available();

/** Whether the caller is still waiting. Asked between the windows of a
    transcription, which is where it can stop; left unset, it never stops early.
*/
using StillWanted = std::function<bool()>;

/** The notes the waveform holds, and nothing at all when this build cannot
    transcribe, when the waveform gives nothing to read, or when the caller
    stopped waiting.

    A waveform of any rate and any number of channels: what the model reads is
    one channel at 22050 Hz, and getting there is this routine's business.
*/
[[nodiscard]] std::optional<Transcribed> transcribed (const analysis::Waveform& waveform,
                                                      const StillWanted& keepGoing = {});

/** Which instrument the waveform sounds like, named out of a small fixed set,
    with how well the evidence fits that name.

    The evidence is the notes that were transcribed out of it — how much of the
    stretch they cover, how low they sit, and how many of them sound at once —
    and the confidence is that fit multiplied by how tonal the spectrum is. So a
    waveform with no shape to its spectrum at all is evidence for nothing, and
    is answered with a name and almost no confidence rather than with a
    confident wrong answer: white noise is the case that matters, since it will
    always look percussive to a routine that only counts notes.
*/
[[nodiscard]] analysis::Estimated instrumentOf (const analysis::Waveform& waveform,
                                                const std::vector<Note>& notes);
} // namespace duet::collab::transcription
