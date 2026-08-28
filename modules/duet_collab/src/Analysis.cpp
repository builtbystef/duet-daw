#include <duet/collab/Analysis.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

namespace duet::collab::analysis
{
namespace
{
    /** A gain as decibels of full scale, with the floor under it: nothing at
        all is a number here rather than an infinity, so a result crosses the
        seam as a scalar whatever the track held.
    */
    double toDecibels (double gain)
    {
        return gain > 0.0 ? std::max (silenceDb, 20.0 * std::log10 (gain)) : silenceDb;
    }

    /** The loudest sample of any channel, as a gain. */
    double peakGain (const Waveform& waveform)
    {
        double peak = 0.0;

        for (const auto& channel : waveform.channels)
            for (const auto sample : channel)
                peak = std::max (peak, std::abs (static_cast<double> (sample)));

        return peak;
    }

    /** How far above its own samples a reconstruction is looked for, and how
        much of the signal each reconstructed point is made of.

        Four times the rate is what BS.1770-4 asks for, and forty-eight taps is
        the length of the filter it prints — twelve of them reaching each
        reconstructed point, which is enough of a sinc for the tenths of a
        decibel this measurement is stated to.
    */
    constexpr int oversampling = 4;
    constexpr int filterLength = 48;

    /** The reconstruction filter: a sinc through the sample points, windowed so
        it can end, and normalised a phase at a time so that each reconstructed
        point carries the signal's own level rather than a fraction of it.
    */
    const std::vector<double>& reconstructionFilter()
    {
        static const std::vector<double> taps = []
        {
            std::vector<double> made (filterLength);
            const auto centre = (filterLength - 1) / 2.0;

            for (int tap = 0; tap < filterLength; ++tap)
            {
                const auto from = (static_cast<double> (tap) - centre) / oversampling;
                const auto sinc = std::abs (from) < 1.0e-9 ? 1.0
                                                           : std::sin (std::numbers::pi * from)
                                                                 / (std::numbers::pi * from);

                // Blackman, whose sidelobes are far enough down that the window
                // is not what limits the answer.
                const auto position = static_cast<double> (tap) / (filterLength - 1);
                const auto window = 0.42 - 0.5 * std::cos (2.0 * std::numbers::pi * position)
                                    + 0.08 * std::cos (4.0 * std::numbers::pi * position);

                made[static_cast<std::size_t> (tap)] = sinc * window;
            }

            for (int phase = 0; phase < oversampling; ++phase)
            {
                double sum = 0.0;

                for (int tap = phase; tap < filterLength; tap += oversampling)
                    sum += made[static_cast<std::size_t> (tap)];

                if (sum != 0.0)
                    for (int tap = phase; tap < filterLength; tap += oversampling)
                        made[static_cast<std::size_t> (tap)] /= sum;
            }

            return made;
        }();

        return taps;
    }

    /** The root mean square over every channel, as a gain. */
    double rmsGain (const Waveform& waveform)
    {
        if (waveform.empty())
            return 0.0;

        double sumOfSquares = 0.0;

        for (const auto& channel : waveform.channels)
            for (const auto sample : channel)
                sumOfSquares += static_cast<double> (sample) * static_cast<double> (sample);

        const auto counted = waveform.length() * waveform.channels.size();

        return std::sqrt (sumOfSquares / static_cast<double> (counted));
    }
} // namespace

//==============================================================================
std::size_t Waveform::length() const
{
    return channels.empty() ? std::size_t { 0 } : channels.front().size();
}

double Waveform::lengthSeconds() const
{
    return sampleRate > 0.0 ? static_cast<double> (length()) / sampleRate : 0.0;
}

bool Waveform::empty() const { return sampleRate <= 0.0 || length() == 0; }

Waveform Waveform::between (double fromSeconds, double toSeconds) const
{
    Waveform cut { sampleRate, {} };

    if (empty())
        return cut;

    const auto toSample = [this] (double seconds)
    {
        if (! (seconds > 0.0))
            return std::size_t { 0 };

        const auto sample = seconds * sampleRate;

        return sample >= static_cast<double> (length()) ? length()
                                                        : static_cast<std::size_t> (sample);
    };

    const auto first = toSample (fromSeconds);
    const auto last = std::max (first, toSample (toSeconds));

    for (const auto& channel : channels)
        cut.channels.emplace_back (channel.begin() + static_cast<std::ptrdiff_t> (first),
                                   channel.begin() + static_cast<std::ptrdiff_t> (last));

    return cut;
}

//==============================================================================
double peakDb (const Waveform& waveform) { return toDecibels (peakGain (waveform)); }

double truePeakDbtp (const Waveform& waveform)
{
    if (waveform.empty())
        return silenceDb;

    const auto& taps = reconstructionFilter();
    const auto length = static_cast<std::ptrdiff_t> (waveform.length());
    double peak = peakGain (waveform);

    for (const auto& channel : waveform.channels)
    {
        for (std::ptrdiff_t point = 0; point < length * oversampling; ++point)
        {
            double reconstructed = 0.0;

            // The taps that reach this point: the filter reads the signal as if
            // it had been stretched by four with zeros in the gaps, so only
            // every fourth tap ever meets a sample.
            for (int tap = static_cast<int> (point % oversampling); tap < filterLength;
                 tap += oversampling)
            {
                const auto sample = (point - tap) / oversampling;

                if (sample >= 0 && sample < length)
                    reconstructed +=
                        taps[static_cast<std::size_t> (tap)]
                        * static_cast<double> (channel[static_cast<std::size_t> (sample)]);
            }

            peak = std::max (peak, std::abs (reconstructed));
        }
    }

    return toDecibels (peak);
}

double rmsDb (const Waveform& waveform) { return toDecibels (rmsGain (waveform)); }

namespace
{
    /** The two sides of a stereo waveform, added up: what each channel carries,
        and what the two carry together.

        A waveform of one channel is that channel twice, which is what makes a
        mono render perfectly correlated with itself and not wide at all rather
        than an answer that has to be special-cased.
    */
    struct StereoSums
    {
        double left = 0.0;
        double right = 0.0;
        double together = 0.0;
        double mid = 0.0;
        double side = 0.0;
    };

    StereoSums stereoSums (const Waveform& waveform)
    {
        StereoSums sums;

        if (waveform.empty())
            return sums;

        const auto& left = waveform.channels.front();
        const auto& right = waveform.channels.size() > 1 ? waveform.channels[1] : left;

        for (std::size_t sample = 0; sample < waveform.length(); ++sample)
        {
            const auto l = static_cast<double> (left[sample]);
            const auto r = static_cast<double> (right[sample]);

            sums.left += l * l;
            sums.right += r * r;
            sums.together += l * r;
            sums.mid += (l + r) * (l + r) / 4.0;
            sums.side += (l - r) * (l - r) / 4.0;
        }

        return sums;
    }
} // namespace

double stereoCorrelation (const Waveform& waveform)
{
    const auto sums = stereoSums (waveform);
    const auto apart = sums.left * sums.right;

    // Nothing to compare: a channel with no signal in it is neither in phase
    // with the other nor out of it.
    if (apart <= 0.0)
        return 0.0;

    return std::clamp (sums.together / std::sqrt (apart), -1.0, 1.0);
}

double stereoWidth (const Waveform& waveform)
{
    const auto sums = stereoSums (waveform);
    const auto whole = sums.mid + sums.side;

    if (whole <= 0.0)
        return 0.0;

    return std::clamp (sums.side / whole, 0.0, 1.0);
}

double crestFactorDb (const Waveform& waveform)
{
    const auto peak = peakGain (waveform);
    const auto rms = rmsGain (waveform);

    // Zero rather than a difference of two floors: silence has no crest, and a
    // negative crest factor is not a thing a waveform can have.
    if (peak <= 0.0 || rms <= 0.0)
        return 0.0;

    return std::max (0.0, 20.0 * std::log10 (peak / rms));
}
} // namespace duet::collab::analysis
