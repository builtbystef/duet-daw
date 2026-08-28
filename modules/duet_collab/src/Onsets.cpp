#include "Fft.h"

#include <duet/collab/Analysis.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

/** Onsets by spectral flux: how much of the spectrum grew from one frame to the
    next, the peaks of that, and then a look at the waveform itself to say when
    each peak actually began.
*/
namespace duet::collab::analysis
{
namespace
{
    /** What the flux is measured over. The window is short enough that the
        sound a peak belongs to is inside it, and the step is short enough that
        a peak names a few milliseconds rather than a few dozen.
    */
    constexpr std::size_t fluxWindow = 1024;
    constexpr std::size_t fluxHop = 256;

    /** How a peak is told from the flux around it: it has to stand above its
        neighbours, above the run of the flux nearby, and above a share of the
        largest flux in the whole waveform, which is what keeps the noise of a
        quiet passage from reading as a run of onsets.
    */
    constexpr std::size_t peakNeighbourhood = 3;
    constexpr std::size_t localWindow = 10;
    constexpr double aboveLocal = 0.12;
    constexpr double aboveLoudest = 0.06;

    /** How close two onsets may be. Under this they are one sound, not two. */
    constexpr double closestOnsetSeconds = 0.05;

    /** How far back from a peak the rise that made it is looked for, and how
        much of the peak's own flux still counts as part of that rise.
    */
    constexpr double partOfTheRise = 0.2;

    /** How finely the waveform itself is read to place an onset: a step this
        long is the whole of the error a placed onset carries.
    */
    constexpr std::size_t envelopeStep = 64;

    std::vector<double> hann (std::size_t length)
    {
        std::vector<double> window (length);

        for (std::size_t sample = 0; sample < length; ++sample)
            window[sample] = 0.5
                             - 0.5
                                   * std::cos (2.0 * std::numbers::pi * static_cast<double> (sample)
                                               / static_cast<double> (length - 1));

        return window;
    }

    std::vector<double> summedToMono (const Waveform& waveform)
    {
        std::vector<double> mono (waveform.length(), 0.0);

        for (const auto& channel : waveform.channels)
            for (std::size_t sample = 0; sample < mono.size(); ++sample)
                mono[sample] += static_cast<double> (channel[sample]);

        return mono;
    }

    /** How much of the spectrum grew, frame by frame. */
    std::vector<double> spectralFlux (const std::vector<double>& mono)
    {
        const auto window = hann (fluxWindow);
        const auto bins = fluxWindow / 2 + 1;

        std::vector<double> flux;
        std::vector<double> previous (bins, 0.0);

        for (std::size_t start = 0; start + fluxWindow <= mono.size(); start += fluxHop)
        {
            std::vector<double> real (fluxWindow);
            std::vector<double> imaginary (fluxWindow, 0.0);

            for (std::size_t sample = 0; sample < fluxWindow; ++sample)
                real[sample] = mono[start + sample] * window[sample];

            fft::forward (real, imaginary);

            double grew = 0.0;

            for (std::size_t bin = 0; bin < bins; ++bin)
            {
                const auto magnitude =
                    std::sqrt (real[bin] * real[bin] + imaginary[bin] * imaginary[bin]);

                // Only what grew: a sound stopping is not a sound starting.
                grew += std::max (0.0, magnitude - previous[bin]);
                previous[bin] = magnitude;
            }

            flux.push_back (grew);
        }

        return flux;
    }

    /** The frames whose flux stands out from the flux around them. */
    std::vector<std::size_t> peaksOf (const std::vector<double>& flux)
    {
        std::vector<std::size_t> peaks;

        if (flux.empty())
            return peaks;

        const auto loudest = *std::max_element (flux.begin(), flux.end());

        if (loudest <= 0.0)
            return peaks;

        // From the first frame, and not from the second: what came before the
        // waveform is silence, so a sound already sounding in its first frame
        // started there as far as anything here can tell. That is true of a
        // render, which begins where the project does; a stretch cut out of one
        // is measured whole and then read for the part that was asked about.
        for (std::size_t frame = 0; frame < flux.size(); ++frame)
        {
            if (flux[frame] < aboveLoudest * loudest)
                continue;

            const auto from = frame > peakNeighbourhood ? frame - peakNeighbourhood : 0;
            const auto to = std::min (flux.size(), frame + peakNeighbourhood + 1);
            auto tallest = true;

            for (auto other = from; other < to && tallest; ++other)
                tallest = other == frame || flux[other] <= flux[frame];

            if (! tallest)
                continue;

            const auto nearFrom = frame > localWindow ? frame - localWindow : 0;
            const auto nearTo = std::min (flux.size(), frame + localWindow + 1);
            double run = 0.0;

            for (auto other = nearFrom; other < nearTo; ++other)
                run += flux[other];

            run /= static_cast<double> (nearTo - nearFrom);

            if (flux[frame] > run + aboveLocal * loudest)
                peaks.push_back (frame);
        }

        return peaks;
    }

    /** The first frame of the rise a peak is the top of: a sound grows over
        several frames, and the one it began in is the one to look at the
        waveform in.
    */
    std::size_t riseBefore (const std::vector<double>& flux, std::size_t peak)
    {
        auto frame = peak;

        while (frame > 0 && flux[frame - 1] < flux[frame]
               && flux[frame - 1] > partOfTheRise * flux[peak])
            --frame;

        return frame;
    }

    /** When, inside one frame's window, the sound actually rose: the step of the
        waveform whose level stands furthest above the step in front of it.
    */
    double roseAt (const std::vector<double>& mono, std::size_t from, double sampleRate)
    {
        const auto to = std::min (mono.size(), from + fluxWindow);

        double loudestRise = -1.0;
        auto rose = from;
        double before = 0.0;
        auto first = true;

        for (auto start = from; start < to; start += envelopeStep)
        {
            double level = 0.0;

            for (auto sample = start; sample < std::min (to, start + envelopeStep); ++sample)
                level = std::max (level, std::abs (mono[sample]));

            if (! first && level - before > loudestRise)
            {
                loudestRise = level - before;
                rose = start;
            }

            before = level;
            first = false;
        }

        return static_cast<double> (rose) / sampleRate;
    }
} // namespace

std::vector<double> onsetsSeconds (const Waveform& waveform)
{
    std::vector<double> onsets;

    if (waveform.empty() || waveform.length() < fluxWindow)
        return onsets;

    const auto mono = summedToMono (waveform);
    const auto flux = spectralFlux (mono);

    for (const auto peak : peaksOf (flux))
    {
        const auto frame = riseBefore (flux, peak);
        const auto at = roseAt (mono, frame * fluxHop, waveform.sampleRate);

        // Two peaks of one sound are one onset: the later of them is the same
        // strike still growing.
        if (! onsets.empty() && at - onsets.back() < closestOnsetSeconds)
            continue;

        onsets.push_back (at);
    }

    return onsets;
}
} // namespace duet::collab::analysis
