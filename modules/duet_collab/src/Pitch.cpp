#include <duet/collab/Analysis.h>

#include <algorithm>
#include <cmath>
#include <vector>

/** Monophonic pitch by YIN: the difference function, the cumulative mean that
    normalises it, the first dip below the threshold, and a parabola through
    that dip's neighbours to find where between two lags it actually fell.
*/
namespace duet::collab::analysis
{
namespace
{
    /** What a frame of the waveform is, and how much of it the difference
        function integrates over. The lag it looks for lives in the rest, so a
        frame holds the window and the longest lag together.
    */
    constexpr std::size_t frameLength = 4096;
    constexpr std::size_t integrationLength = 2048;

    /** The range a musical line is looked for in: the octave below the lowest
        note a bass plays, up to two octaves above the top of a piano.
    */
    constexpr double lowestPitchHz = 40.0;
    constexpr double highestPitchHz = 5000.0;

    /** How far below one a dip has to fall before it counts as the period, and
        how quiet a frame may be before it is not asked at all.
    */
    constexpr double dipThreshold = 0.15;
    constexpr double quietestFrame = 1.0e-4;

    /** How many frames are ever measured, spread over the whole waveform: the
        answer is the middle one of them, and a hundred frames of a three-minute
        track says the same thing as every frame of it for a fraction of the
        work.
    */
    constexpr std::size_t mostFrames = 64;

    /** Every channel added together: YIN is asked about one line, and a line
        panned somewhere is still one line.
    */
    std::vector<double> summedToMono (const Waveform& waveform)
    {
        std::vector<double> mono (waveform.length(), 0.0);

        for (const auto& channel : waveform.channels)
            for (std::size_t sample = 0; sample < mono.size(); ++sample)
                mono[sample] += static_cast<double> (channel[sample]);

        return mono;
    }

    /** The pitch of one frame, and nothing when the frame has none. */
    std::optional<double>
        pitchOfFrame (const std::vector<double>& mono, std::size_t start, double sampleRate)
    {
        const auto lowestLag =
            std::max<std::size_t> (2, static_cast<std::size_t> (sampleRate / highestPitchHz));
        const auto highestLag = std::min (frameLength - integrationLength - 1,
                                          static_cast<std::size_t> (sampleRate / lowestPitchHz));

        if (highestLag <= lowestLag)
            return {};

        std::vector<double> difference (highestLag + 1, 0.0);

        for (auto lag = std::size_t { 1 }; lag <= highestLag; ++lag)
        {
            double squares = 0.0;

            for (std::size_t sample = 0; sample < integrationLength; ++sample)
            {
                const auto apart = mono[start + sample] - mono[start + sample + lag];
                squares += apart * apart;
            }

            difference[lag] = squares;
        }

        // The cumulative mean normalisation: what makes a dip's depth mean the
        // same thing at every lag, and what the threshold below is against.
        std::vector<double> normalised (highestLag + 1, 1.0);
        double running = 0.0;

        for (auto lag = std::size_t { 1 }; lag <= highestLag; ++lag)
        {
            running += difference[lag];
            normalised[lag] =
                running > 0.0 ? difference[lag] * static_cast<double> (lag) / running : 1.0;
        }

        auto found = std::size_t { 0 };

        for (auto lag = lowestLag; lag <= highestLag; ++lag)
            if (normalised[lag] < dipThreshold)
            {
                // Down to the bottom of this dip rather than stopping at its
                // edge, so that the period is the period and not the first lag
                // that was good enough.
                while (lag + 1 <= highestLag && normalised[lag + 1] < normalised[lag])
                    ++lag;

                found = lag;
                break;
            }

        if (found == 0)
            return {};

        // A parabola through the dip and its neighbours: the period falls
        // between two lags, and a cent is finer than a lag is wide.
        auto period = static_cast<double> (found);

        if (found > 1 && found < highestLag)
        {
            const auto before = normalised[found - 1];
            const auto at = normalised[found];
            const auto after = normalised[found + 1];
            const auto curvature = before - 2.0 * at + after;

            if (curvature != 0.0)
                period += 0.5 * (before - after) / curvature;
        }

        return period > 0.0 ? std::optional { sampleRate / period } : std::nullopt;
    }
} // namespace

std::optional<double> pitchHz (const Waveform& waveform)
{
    if (waveform.empty() || waveform.length() < frameLength)
        return {};

    const auto mono = summedToMono (waveform);
    const auto lastStart = mono.size() - frameLength;
    const auto step = std::max<std::size_t> (frameLength, lastStart / mostFrames + 1);

    std::vector<double> found;

    for (std::size_t start = 0; start <= lastStart; start += step)
    {
        double squares = 0.0;

        for (std::size_t sample = 0; sample < frameLength; ++sample)
            squares += mono[start + sample] * mono[start + sample];

        if (std::sqrt (squares / static_cast<double> (frameLength)) < quietestFrame)
            continue;

        if (const auto pitch = pitchOfFrame (mono, start, waveform.sampleRate))
            found.push_back (*pitch);
    }

    if (found.empty())
        return {};

    // The middle one, so that a frame that heard something else does not move
    // the answer.
    const auto middle = found.begin() + static_cast<std::ptrdiff_t> (found.size() / 2);
    std::nth_element (found.begin(), middle, found.end());

    return *middle;
}
} // namespace duet::collab::analysis
