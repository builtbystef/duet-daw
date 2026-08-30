#include <duet/collab/Transcription.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <random>
#include <vector>

/** The transcription and the instrument reading as pure functions of a
    waveform, which is the seam the spec puts every analysis routine at.

    A signal built in this file holds the notes this file says it holds, so what
    the routine should answer is true by construction — and what is not true by
    construction is that a trained model agrees, which is the whole of what
    separates this from the measured routines. So what is asserted is what an
    estimate can carry: the right notes on material that has only one reading,
    times inside the model's own resolution, a confidence in range, and less of
    it for noise than for music.

    Every case here says what it asserts about the build it is in. A build
    without the ML runtime is a supported build, not a broken one: it answers
    that it cannot transcribe, and the case that asserts that is the one that
    runs there.
*/

using Catch::Matchers::WithinAbs;
using duet::collab::analysis::Waveform;
using duet::collab::transcription::Note;
using duet::collab::transcription::Transcribed;

namespace
{
constexpr double rate = 44100.0;

/** How far a start time may fall from where the signal puts it. The model
    answers in frames of 256 samples at 22050 Hz — 11.6 ms — and reads a note as
    beginning in the first frame that heard it, so two frames is its floor and
    not this suite's slack.
*/
constexpr double startTolerance = 0.05;

/** The pitch of a MIDI note, in hertz. */
double frequencyOf (int pitch)
{
    return 440.0 * std::pow (2.0, (static_cast<double> (pitch) - 69.0) / 12.0);
}

/** Every note of a chord sounding together, as sines: middle C is pitch 60, so
    a C major triad is 60, 64, 67.
*/
Waveform chord (const std::vector<int>& pitches, double seconds)
{
    Waveform made { rate, {} };
    const auto samples = static_cast<std::size_t> (seconds * rate);

    for (int channel = 0; channel < 2; ++channel)
    {
        std::vector<float> written (samples, 0.0F);

        for (const auto pitch : pitches)
            for (std::size_t sample = 0; sample < samples; ++sample)
                written[sample] +=
                    static_cast<float> (0.2
                                        * std::sin (2.0 * std::numbers::pi * frequencyOf (pitch)
                                                    * static_cast<double> (sample) / rate));

        made.channels.push_back (std::move (written));
    }

    return made;
}

/** One waveform after another: what a melody is made of. */
Waveform then (const Waveform& first, const Waveform& second)
{
    auto joined = first;

    for (std::size_t channel = 0; channel < joined.channels.size(); ++channel)
        joined.channels[channel].insert (joined.channels[channel].end(),
                                         second.channels[channel].begin(),
                                         second.channels[channel].end());

    return joined;
}

/** One pitch after another, each of the same length: a line, and never two
    notes at once.
*/
Waveform melody (const std::vector<int>& pitches, double secondsEach)
{
    Waveform made = chord ({ pitches.front() }, secondsEach);

    for (std::size_t pitch = 1; pitch < pitches.size(); ++pitch)
        made = then (made, chord ({ pitches[pitch] }, secondsEach));

    return made;
}

/** A stretch of white noise: every frequency at once, and no note in it. */
Waveform noise (double seconds)
{
    Waveform made { rate, {} };
    const auto samples = static_cast<std::size_t> (seconds * rate);
    // A seed of its own rather than the machine's, so that the noise is the
    // same noise on every run: a case that fails one time in ten is worth less
    // than no case at all.
    std::mt19937 source { 20260830U }; // NOLINT(cert-msc32-c,cert-msc51-cpp)
    std::uniform_real_distribution<float> spread { -0.4F, 0.4F };

    for (int channel = 0; channel < 2; ++channel)
    {
        std::vector<float> written (samples, 0.0F);

        for (auto& sample : written)
            sample = spread (source);

        made.channels.push_back (std::move (written));
    }

    return made;
}

/** The notes of that pitch, in the order they start. */
std::vector<Note> of (const std::vector<Note>& notes, int pitch)
{
    std::vector<Note> found;

    for (const auto& note : notes)
        if (note.pitch == pitch)
            found.push_back (note);

    return found;
}

/** Whether this build can transcribe at all, said once so that each case reads
    the same way.
*/
bool transcribes() { return duet::collab::transcription::available(); }

/** What the routine read, with a reading of no notes standing in for a build
    that could not read at all. Every case below has already said which build it
    is in, so the two cannot be confused here.
*/
Transcribed readingOf (const Waveform& waveform)
{
    return duet::collab::transcription::transcribed (waveform).value_or (Transcribed {});
}
} // namespace

TEST_CASE ("a build without the ML runtime says so, and reads no notes at all", "[collab]")
{
    if (transcribes())
        SKIP ("this build has the transcription model");

    REQUIRE_FALSE (
        duet::collab::transcription::transcribed (chord ({ 60, 64, 67 }, 1.0)).has_value());
}

TEST_CASE ("a held C major triad is read as the three notes it is made of", "[collab]")
{
    if (! transcribes())
        SKIP ("this build has no transcription model");

    // Two beats at 120 bpm, which is what the tool's own worked example holds.
    constexpr double heldSeconds = 1.0;

    const auto read = readingOf (chord ({ 60, 64, 67 }, heldSeconds));

    for (const auto pitch : { 60, 64, 67 })
    {
        const auto found = of (read.notes, pitch);

        REQUIRE (found.size() == 1);
        REQUIRE (found.front().startSeconds < startTolerance);
        REQUIRE_THAT (found.front().lengthSeconds, WithinAbs (heldSeconds, 2.0 * startTolerance));
        REQUIRE (found.front().strength > 0.0);
        REQUIRE (found.front().strength <= 1.0);
    }

    // A model is allowed to hear something that is not there; three notes and a
    // whole octave of overtones would be a different routine.
    REQUIRE (read.notes.size() <= 4);
    REQUIRE (read.confidence > 0.0);
    REQUIRE (read.confidence <= 1.0);
}

TEST_CASE ("a monophonic melody is read as its own pitches, in its own order", "[collab]")
{
    if (! transcribes())
        SKIP ("this build has no transcription model");

    constexpr double eachSeconds = 0.5;
    const std::vector<int> played { 62, 65, 69, 72 };

    const auto read = readingOf (melody (played, eachSeconds));

    std::vector<int> heard;
    std::vector<double> started;

    for (const auto& note : read.notes)
    {
        heard.push_back (note.pitch);
        started.push_back (note.startSeconds);
    }

    REQUIRE (heard == played);

    for (std::size_t note = 0; note < played.size(); ++note)
        REQUIRE_THAT (started[note],
                      WithinAbs (static_cast<double> (note) * eachSeconds, startTolerance));
}

TEST_CASE ("what a stretch is played on is named with how well the evidence fits", "[collab]")
{
    if (! transcribes())
        SKIP ("this build has no transcription model");

    const auto played = chord ({ 60, 64, 67 }, 1.0);
    const auto named = duet::collab::transcription::instrumentOf (played, readingOf (played).notes);

    REQUIRE (named.name == "keys or another chordal instrument");
    REQUIRE (named.confidence > 0.5);
    REQUIRE (named.confidence <= 1.0);

    const auto line = melody ({ 62, 65, 69, 72 }, 0.5);

    REQUIRE (duet::collab::transcription::instrumentOf (line, readingOf (line).notes).name
             == "lead or melody instrument");

    const auto low = melody ({ 36, 38, 41, 36 }, 0.5);

    REQUIRE (duet::collab::transcription::instrumentOf (low, readingOf (low).notes).name == "bass");
}

TEST_CASE ("noise is named with almost no confidence, rather than named confidently", "[collab]")
{
    if (! transcribes())
        SKIP ("this build has no transcription model");

    const auto hiss = noise (2.0);
    const auto named = duet::collab::transcription::instrumentOf (hiss, readingOf (hiss).notes);

    // A name still comes back, because the aspect answers with a name or with
    // nothing at all, and the confidence is what says the name is worth little.
    REQUIRE_FALSE (named.name.empty());
    REQUIRE (named.confidence < 0.2);

    const auto played = chord ({ 60, 64, 67 }, 1.0);

    REQUIRE (duet::collab::transcription::instrumentOf (played, readingOf (played).notes).confidence
             > named.confidence);
}
