#include <duet/collab/Analysis.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

/** Loudness per ITU-R BS.1770-4: the K-weighting filter, the gated mean of
    400 ms blocks, and the sliding three-second window short-term loudness is
    read from.

    The standard prints its filter as coefficients for 48 kHz alone, so what is
    written here is the design behind them — a high shelf and a high-pass, each
    stated by its corner, its Q and its gain — evaluated at whatever rate the
    render was made at. At 48 kHz it reproduces the standard's printed
    coefficients to every digit they are printed with, which is what says the
    design is the one the standard means.
*/
namespace duet::collab::analysis
{
namespace
{
    /** BS.1770's own offset, which turns the weighted mean square into LKFS.

        It is the K-weighting's own gain at 1 kHz turned around: a full-scale
        1 kHz sine in one channel reads −3.01 LKFS because of it, and a stereo
        tone reads the level it was written at.
    */
    constexpr double loudnessOffset = -0.691;

    /** The shape of the K-weighting's two stages, from BS.1770-4's own
        derivation of the coefficients it prints.
    */
    constexpr double shelfFrequencyHz = 1681.974450955533;
    constexpr double shelfGainDb = 3.999843853973347;
    constexpr double shelfQ = 0.7071752369554196;
    constexpr double shelfGainSplit = 0.4996667741545416;
    constexpr double highPassFrequencyHz = 38.13547087602444;
    constexpr double highPassQ = 0.5003270373238773;

    /** The gating BS.1770 measures loudness through. */
    constexpr double blockSeconds = 0.4;
    constexpr double blockStepSeconds = 0.1;
    constexpr double relativeGateLu = -10.0;

    /** What short-term loudness is: three seconds, read ten times a second,
        which is the update rate EBU Tech 3341 asks a meter for.
    */
    constexpr double shortTermSeconds = 3.0;
    constexpr double shortTermStepSeconds = 0.1;

    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    };

    /** The K-weighting's first stage: the high shelf that stands for the head. */
    Biquad shelfAt (double sampleRate)
    {
        const auto k = std::tan (std::numbers::pi * shelfFrequencyHz / sampleRate);
        const auto vh = std::pow (10.0, shelfGainDb / 20.0);
        const auto vb = std::pow (vh, shelfGainSplit);
        const auto a0 = 1.0 + k / shelfQ + k * k;

        return { (vh + vb * k / shelfQ + k * k) / a0,
                 2.0 * (k * k - vh) / a0,
                 (vh - vb * k / shelfQ + k * k) / a0,
                 2.0 * (k * k - 1.0) / a0,
                 (1.0 - k / shelfQ + k * k) / a0 };
    }

    /** The second stage: the RLB high-pass that takes the rumble out. */
    Biquad highPassAt (double sampleRate)
    {
        const auto k = std::tan (std::numbers::pi * highPassFrequencyHz / sampleRate);
        const auto a0 = 1.0 + k / highPassQ + k * k;

        return { 1.0, -2.0, 1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / highPassQ + k * k) / a0 };
    }

    void applyInPlace (const Biquad& filter, std::vector<double>& samples)
    {
        double x1 = 0.0;
        double x2 = 0.0;
        double y1 = 0.0;
        double y2 = 0.0;

        for (auto& sample : samples)
        {
            const auto x0 = sample;
            const auto y0 =
                filter.b0 * x0 + filter.b1 * x1 + filter.b2 * x2 - filter.a1 * y1 - filter.a2 * y2;

            x2 = x1;
            x1 = x0;
            y2 = y1;
            y1 = y0;
            sample = y0;
        }
    }

    /** Every channel K-weighted, which is what a loudness is measured over. */
    std::vector<std::vector<double>> weighted (const Waveform& waveform)
    {
        const auto shelf = shelfAt (waveform.sampleRate);
        const auto highPass = highPassAt (waveform.sampleRate);
        std::vector<std::vector<double>> filtered;

        for (const auto& channel : waveform.channels)
        {
            std::vector<double> samples (channel.begin(), channel.end());
            applyInPlace (shelf, samples);
            applyInPlace (highPass, samples);
            filtered.push_back (std::move (samples));
        }

        return filtered;
    }

    /** The mean square of every channel over one stretch, summed.

        The sum and not the mean: BS.1770 weights each channel and adds them, so
        a signal in two channels is 3 LU louder than the same signal in one, and
        every weight a stereo render can need is 1.0.
    */
    double summedMeanSquare (const std::vector<std::vector<double>>& channels,
                             std::size_t from,
                             std::size_t to)
    {
        if (to <= from)
            return 0.0;

        double sum = 0.0;

        for (const auto& channel : channels)
        {
            double squares = 0.0;

            for (auto sample = from; sample < to; ++sample)
                squares += channel[sample] * channel[sample];

            sum += squares / static_cast<double> (to - from);
        }

        return sum;
    }

    double loudnessOf (double summedSquares)
    {
        return summedSquares > 0.0 ? loudnessOffset + 10.0 * std::log10 (summedSquares)
                                   : inaudibleLoudness;
    }

    /** The mean square of every window of a given length and step, in order. */
    std::vector<double> windowEnergies (const Waveform& waveform,
                                        const std::vector<std::vector<double>>& channels,
                                        double windowSeconds,
                                        double stepSeconds)
    {
        std::vector<double> energies;

        const auto window = static_cast<std::size_t> (windowSeconds * waveform.sampleRate);
        const auto step = static_cast<std::size_t> (stepSeconds * waveform.sampleRate);

        if (window == 0 || step == 0 || waveform.length() < window)
            return energies;

        for (std::size_t start = 0; start + window <= waveform.length(); start += step)
            energies.push_back (summedMeanSquare (channels, start, start + window));

        return energies;
    }

    /** The mean of the energies a predicate keeps, and nothing when it keeps
        none: the gated mean the standard integrates over.
    */
    template <typename Keep>
    std::optional<double> gatedMean (const std::vector<double>& energies, Keep keep)
    {
        double sum = 0.0;
        std::size_t counted = 0;

        for (const auto energy : energies)
            if (keep (loudnessOf (energy)))
            {
                sum += energy;
                ++counted;
            }

        if (counted == 0)
            return {};

        return sum / static_cast<double> (counted);
    }
} // namespace

double lufsIntegrated (const Waveform& waveform)
{
    if (waveform.empty())
        return inaudibleLoudness;

    const auto channels = weighted (waveform);
    const auto energies = windowEnergies (waveform, channels, blockSeconds, blockStepSeconds);

    if (energies.empty())
        return inaudibleLoudness;

    // The absolute gate first, then a relative one ten units below what passed
    // it: what the standard integrates is the loud part of a programme, not the
    // silence a programme is surrounded by.
    const auto aboveAbsolute =
        gatedMean (energies, [] (double loudness) { return loudness > inaudibleLoudness; });

    if (! aboveAbsolute.has_value())
        return inaudibleLoudness;

    const auto relativeGate = loudnessOf (*aboveAbsolute) + relativeGateLu;
    const auto gated = gatedMean (
        energies,
        [&] (double loudness) { return loudness > inaudibleLoudness && loudness > relativeGate; });

    return gated.has_value() ? loudnessOf (*gated) : inaudibleLoudness;
}

double lufsShortTermMax (const Waveform& waveform)
{
    if (waveform.empty())
        return inaudibleLoudness;

    const auto channels = weighted (waveform);
    const auto energies =
        windowEnergies (waveform, channels, shortTermSeconds, shortTermStepSeconds);

    if (energies.empty())
        return inaudibleLoudness;

    return loudnessOf (*std::max_element (energies.begin(), energies.end()));
}
} // namespace duet::collab::analysis
