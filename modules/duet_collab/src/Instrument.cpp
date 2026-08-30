#include <duet/collab/Transcription.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

/** What a stretch of audio sounds like it is being played on.

    The one aspect of the analysis layer with no measurement under it at all:
    there is no property of a waveform that says "bass", so this is a reading of
    the notes that came out of it — how much of the stretch they cover, how low
    they sit, and how many sound at once — and it says so, by crossing the seam
    wrapped with a confidence like every other guess (ADR 0002).

    The names it can give are a small fixed set, because a name the model cannot
    act on is worse than a vaguer one it can, and because what this evidence
    supports is a family and not a make: whether a part is the low one, the
    chordal one, the single line on top, or not pitched at all is what a
    register and a polyphony can tell, and a Rhodes from a Wurlitzer is not.

    The confidence is what keeps that honest. It is the fit of the evidence
    multiplied by how tonal the spectrum is, so material with no shape to its
    spectrum — white noise being the case that matters, since it looks
    percussive to anything that only counts notes — is answered with a name and
    almost no confidence rather than with a confident wrong answer.

    This file is compiled whether or not the build has the ML runtime: what
    needs the runtime is the notes, not the reading of them.
*/
namespace duet::collab::transcription
{
namespace
{
    /** The lowest pitch a part has to sit above to be something other than the
        bass: C3, an octave below middle C, which is about where a bass line
        stops and a chord voicing starts.
    */
    constexpr int bassCeiling = 48;

    /** How much of a stretch has to have a note sounding in it before the
        stretch is pitched material at all. Below this it is percussion, or it
        is nothing.
    */
    constexpr double pitchedShare = 0.25;

    /** How many notes have to sound at once, on average, before a part is
        chordal rather than a single line. A shade above one and a half, so that
        a monophonic line with its notes overlapping slightly stays a line.
    */
    constexpr double chordalVoices = 1.6;

    /** How many onsets a second is as struck as the evidence gets: at this rate
        or above, percussion fits as well as it can.
    */
    constexpr double struckOnsetsPerSecond = 2.0;

    /** How long, of the whole stretch, has at least one note sounding in it. */
    double soundingSeconds (const std::vector<Note>& notes)
    {
        std::vector<std::pair<double, double>> spans;
        spans.reserve (notes.size());

        for (const auto& note : notes)
            if (note.lengthSeconds > 0.0)
                spans.emplace_back (note.startSeconds, note.startSeconds + note.lengthSeconds);

        std::sort (spans.begin(), spans.end());

        double sounding = 0.0;
        double from = 0.0;
        double to = 0.0;
        bool open = false;

        for (const auto& span : spans)
        {
            if (open && span.first <= to)
            {
                to = std::max (to, span.second);
                continue;
            }

            if (open)
                sounding += to - from;

            from = span.first;
            to = span.second;
            open = true;
        }

        if (open)
            sounding += to - from;

        return sounding;
    }

    /** The pitch half the notes sit below. */
    int middlePitch (const std::vector<Note>& notes)
    {
        std::vector<int> pitches;
        pitches.reserve (notes.size());

        for (const auto& note : notes)
            pitches.push_back (note.pitch);

        std::sort (pitches.begin(), pitches.end());

        return pitches[pitches.size() / 2];
    }
} // namespace

analysis::Estimated instrumentOf (const analysis::Waveform& waveform,
                                  const std::vector<Note>& notes)
{
    const auto length = waveform.lengthSeconds();

    if (length <= 0.0)
        return {};

    // How much shape the spectrum has, from none to all of it. Everything below
    // is multiplied by this, which is the whole of what keeps noise from being
    // named confidently.
    const auto tonal = std::clamp (1.0 - analysis::spectralFlatness (waveform), 0.0, 1.0);
    const auto sounding = soundingSeconds (notes);
    const auto covered = std::clamp (sounding / length, 0.0, 1.0);

    if (notes.empty() || covered < pitchedShare)
    {
        const auto onsets = static_cast<double> (analysis::onsetsSeconds (waveform).size());
        const auto struck = std::clamp (onsets / (struckOnsetsPerSecond * length), 0.0, 1.0);

        return { "drums or percussion", tonal * struck };
    }

    double played = 0.0;

    for (const auto& note : notes)
        played += note.lengthSeconds;

    const auto voices = sounding > 0.0 ? played / sounding : 0.0;

    if (middlePitch (notes) < bassCeiling)
        return { "bass", tonal * covered };

    if (voices >= chordalVoices)
        return { "keys or another chordal instrument", tonal * covered };

    return { "lead or melody instrument", tonal * covered };
}
} // namespace duet::collab::transcription
