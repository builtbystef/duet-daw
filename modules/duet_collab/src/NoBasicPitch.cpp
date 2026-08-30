#include <duet/collab/Transcription.h>

#include <optional>

/** What stands in for Basic Pitch in a build that left the ML runtime out.

    The spec makes the one ML dependency explicitly separable, and this file is
    what that separation costs: the header is the same header, every caller
    compiles unchanged, and the answer is that there is no answer. A tool asked
    for notes in such a build leaves the aspect out of its result, the same way
    it leaves out an aspect it could not read — and key and chords, which are
    arithmetic over a pitch-class profile and never needed a model, keep working
    exactly as before.
*/
namespace duet::collab::transcription
{
bool available() { return false; }

std::optional<Transcribed> transcribed (const analysis::Waveform& /*waveform*/,
                                        const StillWanted& /*keepGoing*/)
{
    return {};
}
} // namespace duet::collab::transcription
