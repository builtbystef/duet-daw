#include <duet/collab/Analysis.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

/** The analysis routines as pure functions of a waveform — the second seam spec
    js437t names for this area.

    Every signal here is built in this file, so what it measures is true by
    construction rather than by comparison with a recording: a full-scale sine's
    RMS is −3.01 dB because that is what a sine is, and the EBU's published test
    signals measure their published values because the EBU says so.
*/

using Catch::Matchers::WithinAbs;
using duet::collab::analysis::Waveform;

namespace
{
constexpr double rate = 44100.0;

/** A sine of a given amplitude, in as many channels as asked for. */
Waveform sine (double frequencyHz, double amplitude, double seconds, int channelCount = 2)
{
    Waveform made { rate, {} };
    const auto samples = static_cast<std::size_t> (seconds * rate);

    for (int channel = 0; channel < channelCount; ++channel)
    {
        std::vector<float> written (samples);

        for (std::size_t sample = 0; sample < samples; ++sample)
            written[sample] =
                static_cast<float> (amplitude
                                    * std::sin (2.0 * std::numbers::pi * frequencyHz
                                                * static_cast<double> (sample) / rate));

        made.channels.push_back (std::move (written));
    }

    return made;
}

/** A square wave, whose peak and RMS are the same thing. */
Waveform square (double frequencyHz, double amplitude, double seconds, int channelCount = 2)
{
    auto made = sine (frequencyHz, amplitude, seconds, channelCount);

    for (auto& channel : made.channels)
        for (auto& sample : channel)
            sample =
                sample >= 0.0F ? static_cast<float> (amplitude) : static_cast<float> (-amplitude);

    return made;
}
/** A 1 kHz sine at the level EBU Tech 3341 states its test signals at, in both
    channels: the level a stereo tone is written at is the loudness it measures,
    which is what BS.1770's own −0.691 offset exists to make true.
*/
Waveform ebuTone (double dbfs, double seconds)
{
    return sine (1000.0, std::pow (10.0, dbfs / 20.0), seconds);
}

/** One waveform after another, as the multi-part Tech 3341 cases are built. */
Waveform then (const Waveform& first, const Waveform& second)
{
    auto joined = first;

    for (std::size_t channel = 0; channel < joined.channels.size(); ++channel)
        joined.channels[channel].insert (joined.channels[channel].end(),
                                         second.channels[channel].begin(),
                                         second.channels[channel].end());

    return joined;
}
/** A struck sound: a short tone that decays, which is what an onset detector is
    asked about.
*/
void strikeAt (Waveform& into, double atSeconds)
{
    constexpr double decaySeconds = 0.008;
    const auto from = static_cast<std::size_t> (atSeconds * rate);

    for (auto& channel : into.channels)
        for (std::size_t sample = from; sample < channel.size(); ++sample)
        {
            const auto since = static_cast<double> (sample - from) / rate;

            if (since > 20.0 * decaySeconds)
                break;

            channel[sample] +=
                static_cast<float> (std::sin (2.0 * std::numbers::pi * 2000.0 * since)
                                    * std::exp (-since / decaySeconds));
        }
}

/** A waveform of nothing, to put strikes into. */
Waveform silence (double seconds, int channelCount = 2)
{
    return sine (0.0, 0.0, seconds, channelCount);
}
} // namespace

TEST_CASE ("a full-scale sine measures its peak, its RMS and its crest factor", "[analysis]")
{
    const auto tone = sine (1000.0, 1.0, 1.0);

    // The sampling is the whole of the error: at 44.1 kHz no sample of a 1 kHz
    // sine lands exactly on the crest.
    REQUIRE_THAT (duet::collab::analysis::peakDb (tone), WithinAbs (0.0, 0.01));
    REQUIRE_THAT (duet::collab::analysis::rmsDb (tone), WithinAbs (-3.01, 0.01));
    REQUIRE_THAT (duet::collab::analysis::crestFactorDb (tone), WithinAbs (3.01, 0.02));
}

TEST_CASE ("a tone whose crest falls between two samples peaks above them", "[analysis]")
{
    // Every sample of this one lands a quarter cycle either side of the crest,
    // so the file holds ±0.7071 and a converter plays ±1.0. The four-times
    // reconstruction lands exactly on the crest, which is what makes this the
    // signal to state true peak against.
    const auto tone = sine (rate / 4.0, 1.0, 0.5);
    auto shifted = tone;

    for (auto& channel : shifted.channels)
        for (std::size_t sample = 0; sample < channel.size(); ++sample)
            channel[sample] = static_cast<float> (
                std::sin (2.0 * std::numbers::pi * 0.25 * static_cast<double> (sample)
                          + std::numbers::pi / 4.0));

    REQUIRE_THAT (duet::collab::analysis::peakDb (shifted), WithinAbs (-3.01, 0.01));
    REQUIRE_THAT (duet::collab::analysis::truePeakDbtp (shifted), WithinAbs (0.0, 0.3));
}

TEST_CASE ("true peak is never under the sample peak", "[analysis]")
{
    const auto tone = sine (100.0, 0.5, 0.5);

    REQUIRE (duet::collab::analysis::truePeakDbtp (tone)
             >= duet::collab::analysis::peakDb (tone) - 0.0001);
    REQUIRE_THAT (duet::collab::analysis::truePeakDbtp (tone), WithinAbs (-6.02, 0.2));
}

TEST_CASE ("a square wave is all crest and no peak above it", "[analysis]")
{
    const auto tone = square (1000.0, 1.0, 1.0);

    REQUIRE_THAT (duet::collab::analysis::crestFactorDb (tone), WithinAbs (0.0, 0.01));
}

TEST_CASE ("silence measures the floor, and no crest at all", "[analysis]")
{
    const auto nothing = sine (1000.0, 0.0, 1.0);

    REQUIRE_THAT (duet::collab::analysis::peakDb (nothing),
                  WithinAbs (duet::collab::analysis::silenceDb, 0.0001));
    REQUIRE_THAT (duet::collab::analysis::rmsDb (nothing),
                  WithinAbs (duet::collab::analysis::silenceDb, 0.0001));
    REQUIRE_THAT (duet::collab::analysis::crestFactorDb (nothing), WithinAbs (0.0, 0.0001));
}

TEST_CASE ("a stretch of a waveform is measured over what is in that stretch", "[analysis]")
{
    auto quietThenLoud = sine (1000.0, 1.0, 2.0);

    for (auto& channel : quietThenLoud.channels)
        for (std::size_t sample = 0; sample < static_cast<std::size_t> (rate); ++sample)
            channel[sample] = 0.0F;

    REQUIRE_THAT (duet::collab::analysis::peakDb (quietThenLoud.between (0.0, 1.0)),
                  WithinAbs (duet::collab::analysis::silenceDb, 0.0001));
    REQUIRE_THAT (duet::collab::analysis::peakDb (quietThenLoud.between (1.0, 2.0)),
                  WithinAbs (0.0, 0.01));

    // A stretch that reaches past the end is measured over what is there.
    REQUIRE_THAT (duet::collab::analysis::rmsDb (quietThenLoud.between (1.0, 99.0)),
                  WithinAbs (-3.01, 0.01));
}

TEST_CASE ("EBU Tech 3341 case 1: a −23 LUFS tone measures −23, integrated and short-term",
           "[analysis]")
{
    const auto signal = ebuTone (-23.0, 20.0);

    REQUIRE_THAT (duet::collab::analysis::lufsIntegrated (signal), WithinAbs (-23.0, 0.1));
    REQUIRE_THAT (duet::collab::analysis::lufsShortTermMax (signal), WithinAbs (-23.0, 0.1));
}

TEST_CASE ("EBU Tech 3341 case 2: a −33 LUFS tone measures −33", "[analysis]")
{
    const auto signal = ebuTone (-33.0, 20.0);

    REQUIRE_THAT (duet::collab::analysis::lufsIntegrated (signal), WithinAbs (-33.0, 0.1));
    REQUIRE_THAT (duet::collab::analysis::lufsShortTermMax (signal), WithinAbs (-33.0, 0.1));
}

TEST_CASE ("EBU Tech 3341 case 3: the relative gate leaves the quiet parts out", "[analysis]")
{
    const auto signal =
        then (then (ebuTone (-36.0, 10.0), ebuTone (-23.0, 60.0)), ebuTone (-36.0, 10.0));

    REQUIRE_THAT (duet::collab::analysis::lufsIntegrated (signal), WithinAbs (-23.0, 0.1));
    REQUIRE_THAT (duet::collab::analysis::lufsShortTermMax (signal), WithinAbs (-23.0, 0.1));
}

TEST_CASE ("EBU Tech 3341 case 4: the absolute gate leaves the near-silence out", "[analysis]")
{
    const auto quiet = ebuTone (-72.0, 10.0);
    const auto middling = ebuTone (-36.0, 10.0);
    const auto signal =
        then (then (then (then (quiet, middling), ebuTone (-23.0, 60.0)), middling), quiet);

    REQUIRE_THAT (duet::collab::analysis::lufsIntegrated (signal), WithinAbs (-23.0, 0.1));
}

TEST_CASE ("EBU Tech 3341 case 5: a signal that never leaves the gates integrates to −23",
           "[analysis]")
{
    const auto signal =
        then (then (ebuTone (-26.0, 20.0), ebuTone (-20.0, 20.1)), ebuTone (-26.0, 20.0));

    REQUIRE_THAT (duet::collab::analysis::lufsIntegrated (signal), WithinAbs (-23.0, 0.1));
}

TEST_CASE ("silence has no loudness to integrate", "[analysis]")
{
    const auto nothing = sine (1000.0, 0.0, 10.0);

    REQUIRE_THAT (duet::collab::analysis::lufsIntegrated (nothing),
                  WithinAbs (duet::collab::analysis::inaudibleLoudness, 0.0001));
    REQUIRE_THAT (duet::collab::analysis::lufsShortTermMax (nothing),
                  WithinAbs (duet::collab::analysis::inaudibleLoudness, 0.0001));
}

TEST_CASE ("YIN measures a tone's pitch to within a cent", "[analysis]")
{
    const auto measured = duet::collab::analysis::pitchHz (sine (440.0, 0.8, 1.0));

    REQUIRE (measured.has_value());

    // A cent is a hundredth of a semitone, which at this pitch is a quarter of
    // a hertz.
    const auto cents = 1200.0 * std::log2 (measured.value_or (0.0) / 440.0);

    REQUIRE_THAT (cents, WithinAbs (0.0, 1.0));
}

TEST_CASE ("YIN names no pitch for silence rather than the wrong one", "[analysis]")
{
    REQUIRE_FALSE (duet::collab::analysis::pitchHz (sine (440.0, 0.0, 1.0)).has_value());
}

TEST_CASE ("YIN follows a tone wherever it is put", "[analysis]")
{
    for (const auto frequency : { 82.41, 220.0, 1046.5 })
    {
        const auto measured = duet::collab::analysis::pitchHz (sine (frequency, 0.5, 1.0));

        REQUIRE (measured.has_value());
        REQUIRE_THAT (1200.0 * std::log2 (measured.value_or (0.0) / frequency),
                      WithinAbs (0.0, 1.0));
    }
}

TEST_CASE ("a signal that is the same in both channels is fully correlated and not wide",
           "[analysis]")
{
    const auto both = sine (440.0, 0.7, 0.5);

    REQUIRE_THAT (duet::collab::analysis::stereoCorrelation (both), WithinAbs (1.0, 0.0001));
    REQUIRE_THAT (duet::collab::analysis::stereoWidth (both), WithinAbs (0.0, 0.0001));
}

TEST_CASE ("a channel against its own inversion is fully out of phase", "[analysis]")
{
    auto opposed = sine (440.0, 0.7, 0.5);

    for (auto& sample : opposed.channels.at (1))
        sample = -sample;

    REQUIRE_THAT (duet::collab::analysis::stereoCorrelation (opposed), WithinAbs (-1.0, 0.0001));
}

TEST_CASE ("a mono waveform is correlated with itself and has no width", "[analysis]")
{
    const auto alone = sine (440.0, 0.7, 0.5, 1);

    REQUIRE_THAT (duet::collab::analysis::stereoCorrelation (alone), WithinAbs (1.0, 0.0001));
    REQUIRE_THAT (duet::collab::analysis::stereoWidth (alone), WithinAbs (0.0, 0.0001));
}

TEST_CASE ("two unrelated tones are neither correlated nor narrow", "[analysis]")
{
    auto apart = sine (440.0, 0.7, 0.5);
    const auto other = sine (997.0, 0.7, 0.5);
    apart.channels.at (1) = other.channels.at (0);

    REQUIRE_THAT (duet::collab::analysis::stereoCorrelation (apart), WithinAbs (0.0, 0.05));
    REQUIRE (duet::collab::analysis::stereoWidth (apart) > 0.4);
}

TEST_CASE ("silence has nothing to say about its channels", "[analysis]")
{
    const auto nothing = sine (440.0, 0.0, 0.5);

    REQUIRE_THAT (duet::collab::analysis::stereoCorrelation (nothing), WithinAbs (0.0, 0.0001));
    REQUIRE_THAT (duet::collab::analysis::stereoWidth (nothing), WithinAbs (0.0, 0.0001));
}

TEST_CASE ("a tone inside a band puts its energy there and nowhere else", "[analysis]")
{
    const auto& bands = duet::collab::analysis::spectralBands;

    for (std::size_t band = 0; band < bands.size(); ++band)
    {
        // The middle of the band on a logarithmic scale, which is the middle of
        // a band as an ear hears one.
        const auto middle = std::sqrt (bands.at (band).fromHz * bands.at (band).toHz);
        const auto measured =
            duet::collab::analysis::spectralBandEnergiesDb (sine (middle, 1.0, 1.0));

        REQUIRE (measured.size() == bands.size());
        REQUIRE_THAT (measured[band], WithinAbs (-3.01, 0.5));

        for (std::size_t other = 0; other < bands.size(); ++other)
            if (other != band)
                REQUIRE (measured[other] < measured[band] - 40.0);
    }
}

TEST_CASE ("the spectrum's weight sits where the tone is", "[analysis]")
{
    REQUIRE_THAT (duet::collab::analysis::spectralCentroidHz (sine (1000.0, 0.8, 1.0)),
                  WithinAbs (1000.0, 20.0));
    REQUIRE_THAT (duet::collab::analysis::spectralCentroidHz (sine (0.0, 0.0, 1.0)),
                  WithinAbs (0.0, 0.0001));
}

TEST_CASE ("a tone is not flat and noise is", "[analysis]")
{
    Waveform noise { rate, {} };
    unsigned int seed = 1;

    for (int channel = 0; channel < 2; ++channel)
    {
        std::vector<float> written (static_cast<std::size_t> (rate));

        for (auto& sample : written)
        {
            seed = seed * 1664525U + 1013904223U;
            sample =
                static_cast<float> (static_cast<double> (seed) / 4294967295.0 * 2.0 - 1.0) * 0.5F;
        }

        noise.channels.push_back (std::move (written));
    }

    REQUIRE (duet::collab::analysis::spectralFlatness (sine (1000.0, 0.8, 1.0)) < 0.01);
    REQUIRE (duet::collab::analysis::spectralFlatness (noise) > 0.4);
}

TEST_CASE ("onsets land where the strikes are, and nowhere else", "[analysis]")
{
    const std::vector struck { 0.5, 1.0, 1.5, 2.0 };
    auto pattern = silence (2.5);

    for (const auto at : struck)
        strikeAt (pattern, at);

    const auto found = duet::collab::analysis::onsetsSeconds (pattern);

    REQUIRE (found.size() == struck.size());

    for (std::size_t onset = 0; onset < struck.size(); ++onset)
        REQUIRE_THAT (found[onset], WithinAbs (struck[onset], 0.005));
}

TEST_CASE ("a strike over a sounding tone is an onset too", "[analysis]")
{
    auto over = sine (220.0, 0.3, 2.0);
    strikeAt (over, 1.0);

    const auto found = duet::collab::analysis::onsetsSeconds (over);

    REQUIRE (found.size() == 2);
    REQUIRE_THAT (found.at (0), WithinAbs (0.0, 0.02));
    REQUIRE_THAT (found.at (1), WithinAbs (1.0, 0.005));
}

TEST_CASE ("silence starts nothing", "[analysis]")
{
    REQUIRE (duet::collab::analysis::onsetsSeconds (silence (2.0)).empty());
}
