#include "Spectrum.h"

#include "Fft.h"

#include <duet/collab/Analysis.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

/** The spectral routines: what a waveform's energy is spread over, measured by
    an averaged periodogram — successive windows transformed, their power
    spectra averaged, and the bands, the centroid and the flatness read off the
    average.

    That average is also what the estimating routines read their pitch classes
    off, so the transform itself is declared in `Spectrum.h` and made here.
*/
namespace duet::collab::analysis
{
namespace
{
    /** The longest window a spectrum is measured over, and the shortest.

        Long, because the lowest band edge is 60 Hz and a window has to tell
        34 Hz from 60 Hz to keep a sub tone out of the band above it: at 44.1 kHz
        this window's bins are under three hertz apart, and its window's
        sidelobes are ninety decibels down, so a tone in the middle of a band
        leaves nothing measurable in the next one.
    */
    constexpr std::size_t longestWindow = 16384;
    constexpr std::size_t shortestWindow = 256;

    /** The stretch of the spectrum flatness is judged over: what a producer can
        hear, rather than the rumble and the ultrasonics either side of it.
    */
    constexpr double audibleFromHz = 20.0;
    constexpr double audibleToHz = 20000.0;

    /** Under this a bin holds nothing, and a geometric mean would be dragged to
        zero by it. A hundred and eighty decibels down is far below anything a
        render can carry.
    */
    constexpr double smallestPower = 1.0e-18;

    /** The window that keeps a band's tone out of the band beside it: four-term
        Blackman-Harris, whose sidelobes are 92 dB down.
    */
    std::vector<double> blackmanHarris (std::size_t length)
    {
        std::vector<double> window (length);

        for (std::size_t sample = 0; sample < length; ++sample)
        {
            const auto turn = 2.0 * std::numbers::pi * static_cast<double> (sample)
                              / static_cast<double> (length - 1);
            window[sample] = 0.35875 - 0.48829 * std::cos (turn) + 0.14128 * std::cos (2.0 * turn)
                             - 0.01168 * std::cos (3.0 * turn);
        }

        return window;
    }
} // namespace

namespace spectrum
{
    std::size_t windowFor (const Waveform& waveform)
    {
        auto window = longestWindow;

        while (window > waveform.length())
            window >>= 1;

        return window >= shortestWindow ? window : 0;
    }

    std::vector<double> power (const Waveform& waveform, std::size_t window)
    {
        const auto shape = blackmanHarris (window);
        double windowPower = 0.0;

        for (const auto weight : shape)
            windowPower += weight * weight;

        // The scale that makes the bins of a band add up to the mean square of
        // what that band holds. It is the window's own power and not its sum:
        // a tone spreads over the few bins of the window's main lobe, so what
        // has to come out right is the total across them rather than the tallest
        // one of them. Getting that wrong reads every band three decibels loud.
        const auto scale = static_cast<double> (window) * windowPower;

        const auto bins = window / 2 + 1;
        std::vector<double> average (bins, 0.0);
        std::size_t counted = 0;

        const auto hop = window / 2;

        for (const auto& channel : waveform.channels)
        {
            for (std::size_t start = 0; start + window <= channel.size(); start += hop)
            {
                std::vector<double> real (window);
                std::vector<double> imaginary (window, 0.0);

                for (std::size_t sample = 0; sample < window; ++sample)
                    real[sample] = static_cast<double> (channel[start + sample]) * shape[sample];

                fft::forward (real, imaginary);

                for (std::size_t bin = 0; bin < bins; ++bin)
                {
                    // Twice, for the half of the spectrum that is not looked at,
                    // and not for the two bins that have no twin.
                    const auto both = (bin == 0 || bin == window / 2) ? 1.0 : 2.0;
                    const auto magnitude = real[bin] * real[bin] + imaginary[bin] * imaginary[bin];

                    average[bin] += both * magnitude / scale;
                }

                ++counted;
            }
        }

        if (counted == 0)
            return {};

        for (auto& bin : average)
            bin /= static_cast<double> (counted);

        return average;
    }

    double binFrequency (std::size_t bin, std::size_t window, double sampleRate)
    {
        return static_cast<double> (bin) * sampleRate / static_cast<double> (window);
    }
} // namespace spectrum

std::vector<double> spectralBandEnergiesDb (const Waveform& waveform)
{
    std::vector<double> energies (spectralBands.size(), silenceDb);
    const auto window = spectrum::windowFor (waveform);

    if (window == 0)
        return energies;

    const auto bins = spectrum::power (waveform, window);

    if (bins.empty())
        return energies;

    for (std::size_t band = 0; band < spectralBands.size(); ++band)
    {
        double power = 0.0;

        for (std::size_t bin = 0; bin < bins.size(); ++bin)
        {
            const auto frequency = spectrum::binFrequency (bin, window, waveform.sampleRate);

            if (frequency >= spectralBands.at (band).fromHz
                && frequency < spectralBands.at (band).toHz)
                power += bins[bin];
        }

        energies[band] = power > 0.0 ? std::max (silenceDb, 10.0 * std::log10 (power)) : silenceDb;
    }

    return energies;
}

double spectralCentroidHz (const Waveform& waveform)
{
    const auto window = spectrum::windowFor (waveform);

    if (window == 0)
        return 0.0;

    const auto bins = spectrum::power (waveform, window);
    double weighted = 0.0;
    double total = 0.0;

    for (std::size_t bin = 0; bin < bins.size(); ++bin)
    {
        const auto magnitude = std::sqrt (bins[bin]);
        weighted += spectrum::binFrequency (bin, window, waveform.sampleRate) * magnitude;
        total += magnitude;
    }

    return total > 0.0 ? weighted / total : 0.0;
}

double spectralFlatness (const Waveform& waveform)
{
    const auto window = spectrum::windowFor (waveform);

    if (window == 0)
        return 0.0;

    const auto bins = spectrum::power (waveform, window);

    double logSum = 0.0;
    double sum = 0.0;
    std::size_t counted = 0;

    for (std::size_t bin = 0; bin < bins.size(); ++bin)
    {
        const auto frequency = spectrum::binFrequency (bin, window, waveform.sampleRate);

        if (frequency < audibleFromHz || frequency > audibleToHz)
            continue;

        const auto power = std::max (smallestPower, bins[bin]);
        logSum += std::log (power);
        sum += power;
        ++counted;
    }

    if (counted == 0 || sum <= 0.0)
        return 0.0;

    const auto geometric = std::exp (logSum / static_cast<double> (counted));
    const auto arithmetic = sum / static_cast<double> (counted);

    return std::clamp (geometric / arithmetic, 0.0, 1.0);
}
} // namespace duet::collab::analysis
