#include "Spectrum.h"

#include <duet/collab/Analysis.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

/** The estimating routines: what notes a waveform is probably made of, read as
    pitch classes and named as a key or a chord (spec js437t, tier 3).

    Both are the same two steps. A pitch-class profile — how much of each of the
    twelve notes the waveform holds, whatever octave it is in — is read off the
    spectrum; that profile is then correlated with each of a set of published
    templates, and the template it fits best is the name, with the strength of
    the fit as the confidence. What comes out is a guess, and it is a guess
    whose strength is stated, which is what lets it cross the seam wrapped.
*/
namespace duet::collab::analysis
{
namespace
{
    /** How much of each pitch class a waveform holds, C first. */
    using Chroma = std::array<double, 12>;

    /** The stretch of the spectrum notes are read from: the bottom of an
        electric bass at one end, and at the other the point above which what a
        bin holds is a harmonic of something lower rather than a note anybody
        played.
    */
    constexpr double lowestNoteHz = 55.0;
    constexpr double highestNoteHz = 5000.0;

    /** Krumhansl and Kessler's published key profiles: how much of each degree
        a listener hears in a major and in a minor key, the tonic first.
    */
    constexpr std::array<double, 12> majorKeyProfile { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09,
                                                       2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    constexpr std::array<double, 12> minorKeyProfile { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53,
                                                       2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

    /** A triad, as the three degrees it is made of and the nine it is not. */
    constexpr std::array<double, 12> majorTriad { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0,
                                                  0.0, 1.0, 0.0, 0.0, 0.0, 0.0 };
    constexpr std::array<double, 12> minorTriad { 1.0, 0.0, 0.0, 1.0, 0.0, 0.0,
                                                  0.0, 1.0, 0.0, 0.0, 0.0, 0.0 };

    /** The twelve pitch classes as a producer writes them, sharps throughout, so
        that one note has one name wherever it is read.
    */
    constexpr std::array<const char*, 12> pitchClassNames { "C",  "C#", "D",  "D#", "E",  "F",
                                                            "F#", "G",  "G#", "A",  "A#", "B" };

    /** How much of each pitch class the waveform holds.

        Every bin of the average spectrum is put in the pitch class its
        frequency belongs to, and what a class ends up with is the mean
        magnitude of its bins rather than their total. The mean is what makes
        noise read as flat: a class high up owns more bins than a class low
        down, so a total would say something about the piano's layout rather
        than about the music.
    */
    Chroma chromaOf (const Waveform& waveform)
    {
        Chroma held {};
        std::array<std::size_t, 12> counted {};

        const auto window = spectrum::windowFor (waveform);

        if (window == 0)
            return held;

        const auto bins = spectrum::power (waveform, window);

        for (std::size_t bin = 0; bin < bins.size(); ++bin)
        {
            const auto frequency = spectrum::binFrequency (bin, window, waveform.sampleRate);

            if (frequency < lowestNoteHz || frequency > highestNoteHz)
                continue;

            // The MIDI pitch of that frequency, rounded to the note it is
            // nearest, and then the note's own class: pitch 60 is middle C, and
            // twelve of them make an octave, so the remainder is the class.
            const auto pitch = 69.0 + 12.0 * std::log2 (frequency / 440.0);
            const auto pitchClass =
                static_cast<std::size_t> (((std::lround (pitch) % 12) + 12) % 12);

            held.at (pitchClass) += std::sqrt (bins[bin]);
            ++counted.at (pitchClass);
        }

        for (std::size_t pitchClass = 0; pitchClass < held.size(); ++pitchClass)
            if (counted.at (pitchClass) > 0)
                held.at (pitchClass) /= static_cast<double> (counted.at (pitchClass));

        return held;
    }

    /** Pearson's correlation of a profile with a chroma read from `tonic`, which
        is what says how well the one describes the other whatever either is
        scaled by. Zero when there is nothing to correlate.
    */
    double correlation (const Chroma& held, const std::array<double, 12>& profile, int tonic)
    {
        double heldMean = 0.0;
        double profileMean = 0.0;

        for (std::size_t degree = 0; degree < profile.size(); ++degree)
        {
            heldMean += held.at ((static_cast<std::size_t> (tonic) + degree) % 12);
            profileMean += profile.at (degree);
        }

        heldMean /= static_cast<double> (profile.size());
        profileMean /= static_cast<double> (profile.size());

        double covariance = 0.0;
        double heldSpread = 0.0;
        double profileSpread = 0.0;

        for (std::size_t degree = 0; degree < profile.size(); ++degree)
        {
            const auto heldOff =
                held.at ((static_cast<std::size_t> (tonic) + degree) % 12) - heldMean;
            const auto profileOff = profile.at (degree) - profileMean;

            covariance += heldOff * profileOff;
            heldSpread += heldOff * heldOff;
            profileSpread += profileOff * profileOff;
        }

        if (heldSpread <= 0.0 || profileSpread <= 0.0)
            return 0.0;

        return covariance / std::sqrt (heldSpread * profileSpread);
    }

    /** The best fit of a chroma against a pair of templates, one rooted at each
        of the twelve pitch classes, and what to call it.

        `majorSuffix` and `minorSuffix` are what the name says after the note,
        so the same search names a key and a chord.
    */
    Estimated bestFit (const Chroma& held,
                       const std::array<double, 12>& major,
                       const std::array<double, 12>& minor,
                       const std::string& majorSuffix,
                       const std::string& minorSuffix)
    {
        Estimated best;
        double strongest = 0.0;

        for (int root = 0; root < 12; ++root)
        {
            for (const auto minorMode : { false, true })
            {
                const auto fit = correlation (held, minorMode ? minor : major, root);

                if (fit > strongest)
                {
                    strongest = fit;
                    best.name = std::string { pitchClassNames.at (static_cast<std::size_t> (root)) }
                                + (minorMode ? minorSuffix : majorSuffix);
                    best.confidence = std::clamp (fit, 0.0, 1.0);
                }
            }
        }

        return best;
    }
} // namespace

Estimated estimatedKey (const Waveform& waveform)
{
    return bestFit (chromaOf (waveform), majorKeyProfile, minorKeyProfile, " major", " minor");
}

Estimated estimatedChord (const Waveform& waveform)
{
    return bestFit (chromaOf (waveform), majorTriad, minorTriad, " major", " minor");
}
} // namespace duet::collab::analysis
