#include <duet/collab/Analysis.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

/** The estimating routines as pure functions of a waveform — the same seam the
    measured routines are asserted at, and the same reason: a signal built in
    this file holds the notes this file says it holds, so what the routine
    should answer is true by construction.

    What is not true by construction is that the routine is right, which is what
    separates tier 3 from tier 2: a key is a reading of the notes and not a
    property of the waveform. So the assertions here are the ones an estimate
    can carry — the right answer on material that has only one, a confidence
    inside its stated range, and more confidence in music than in noise.
*/

using Catch::Matchers::WithinAbs;
using duet::collab::analysis::Waveform;

namespace
{
constexpr double rate = 44100.0;

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

/** One waveform after another: what a progression is made of. */
Waveform then (const Waveform& first, const Waveform& second)
{
    auto joined = first;

    for (std::size_t channel = 0; channel < joined.channels.size(); ++channel)
        joined.channels[channel].insert (joined.channels[channel].end(),
                                         second.channels[channel].begin(),
                                         second.channels[channel].end());

    return joined;
}

/** The four chords of a I–IV–V–I in C major, which names one key and no other. */
Waveform cMajorProgression()
{
    return then (then (chord ({ 60, 64, 67 }, 1.0), chord ({ 65, 69, 72 }, 1.0)),
                 then (chord ({ 67, 71, 74 }, 1.0), chord ({ 60, 64, 67 }, 1.0)));
}

/** Noise: every pitch class at once, which is to say no key at all. The
    generator is this file's own, so the signal is the same on every machine.
*/
Waveform whiteNoise (double seconds)
{
    Waveform made { rate, {} };
    const auto samples = static_cast<std::size_t> (seconds * rate);
    std::uint32_t state = 0x1234567U;

    for (int channel = 0; channel < 2; ++channel)
    {
        std::vector<float> written (samples);

        for (std::size_t sample = 0; sample < samples; ++sample)
        {
            state = state * 1664525U + 1013904223U;
            written[sample] = static_cast<float> (state) / 2147483648.0F - 1.0F;
        }

        made.channels.push_back (std::move (written));
    }

    return made;
}

Waveform silence (double seconds)
{
    return Waveform { rate,
                      { std::vector<float> (static_cast<std::size_t> (seconds * rate), 0.0F),
                        std::vector<float> (static_cast<std::size_t> (seconds * rate), 0.0F) } };
}
} // namespace

TEST_CASE ("a progression in C major is estimated as C major, and noise is estimated as less",
           "[analysis]")
{
    const auto estimated = duet::collab::analysis::estimatedKey (cMajorProgression());

    REQUIRE (estimated.name == "C major");
    REQUIRE (estimated.confidence > 0.0);
    REQUIRE (estimated.confidence <= 1.0);

    // Noise has every pitch class in it, so whatever key it fits best it fits
    // badly — and that, not the name it lands on, is what the confidence says.
    const auto noise = duet::collab::analysis::estimatedKey (whiteNoise (4.0));

    REQUIRE (noise.confidence >= 0.0);
    REQUIRE (noise.confidence <= 1.0);
    REQUIRE (estimated.confidence > noise.confidence);
}

TEST_CASE ("a minor progression is estimated as its own key, not as its relative major",
           "[analysis]")
{
    // i–iv–V–i in A minor: the E major chord's G sharp is the leading note C
    // major does not have, which is what tells the two apart.
    const auto progression = then (then (chord ({ 57, 60, 64 }, 1.0), chord ({ 62, 65, 69 }, 1.0)),
                                   then (chord ({ 64, 68, 71 }, 1.0), chord ({ 57, 60, 64 }, 1.0)));

    REQUIRE (duet::collab::analysis::estimatedKey (progression).name == "A minor");
}

TEST_CASE ("a triad is estimated as the chord it is", "[analysis]")
{
    REQUIRE (duet::collab::analysis::estimatedChord (chord ({ 60, 64, 67 }, 1.0)).name
             == "C major");
    REQUIRE (duet::collab::analysis::estimatedChord (chord ({ 67, 71, 74 }, 1.0)).name
             == "G major");
    REQUIRE (duet::collab::analysis::estimatedChord (chord ({ 57, 60, 64 }, 1.0)).name
             == "A minor");

    const auto major = duet::collab::analysis::estimatedChord (chord ({ 60, 64, 67 }, 1.0));

    REQUIRE (major.confidence > 0.0);
    REQUIRE (major.confidence <= 1.0);
}

TEST_CASE ("a chord in another octave and another voicing is the same chord", "[analysis]")
{
    // First inversion, an octave up, with the root doubled below: a chord is
    // its pitch classes, and nothing in the routine may depend on which of them
    // is at the bottom.
    REQUIRE (duet::collab::analysis::estimatedChord (chord ({ 48, 64, 67, 72 }, 1.0)).name
             == "C major");
}

TEST_CASE ("silence has no key and no chord, rather than the wrong one", "[analysis]")
{
    const auto quiet = silence (2.0);

    REQUIRE (duet::collab::analysis::estimatedKey (quiet).name.empty());
    REQUIRE_THAT (duet::collab::analysis::estimatedKey (quiet).confidence, WithinAbs (0.0, 1e-9));
    REQUIRE (duet::collab::analysis::estimatedChord (quiet).name.empty());
    REQUIRE_THAT (duet::collab::analysis::estimatedChord (quiet).confidence, WithinAbs (0.0, 1e-9));
}

TEST_CASE ("a waveform too short to transform is estimated as nothing", "[analysis]")
{
    REQUIRE (duet::collab::analysis::estimatedKey (chord ({ 60, 64, 67 }, 0.001)).name.empty());
    REQUIRE (duet::collab::analysis::estimatedChord (chord ({ 60, 64, 67 }, 0.001)).name.empty());
}
