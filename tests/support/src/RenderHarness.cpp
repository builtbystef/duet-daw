#include <duet/testing/RenderHarness.h>

#include <duet/testing/TestSupport.h>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <functional>
#include <numbers>
#include <span>
#include <string>
#include <thread>
#include <utility>

namespace duet::testing
{
namespace
{
    /** Above this a render is making a sound, and how long it must have been
        quiet before that counts as a new one. A hundredth of full scale is well
        clear of what a render of nothing measures, and a tenth of a second is
        shorter than any gap a test puts between two sounds.
    */
    constexpr double onsetLevel = 0.01;
    constexpr double onsetQuietSeconds = 0.1;

    /** How much of a render is looked at at once when walking back to where a
        sound began: longer than the half cycle of any tone a test renders, so
        that the tone's own passage through zero is not mistaken for the silence
        in front of it.
    */
    constexpr double onsetEnvelopeSeconds = 0.004;

    /** How long a stretch has to be before a pitch can be counted from it: two
        rising crossings measure nothing, and the count is what the answer is
        made of.
    */
    constexpr int fewestCrossings = 3;

    /** What a level of nothing reads as, so that a change from or to silence is
        a number rather than an infinity. The model's meters use the same floor.
    */
    constexpr double silentDb = -100.0;

    juce::File toJuceFile (const std::filesystem::path& path)
    {
        return juce::File { juce::String { path.string() } };
    }

    double toDecibels (double gain)
    {
        return gain > 0.0 ? std::max (silentDb, 20.0 * std::log10 (gain)) : silentDb;
    }

    /** Renders on a worker thread while the message loop runs on this one.

        Both halves are the point. The thread model puts an offline render on a
        worker thread, and the engine builds the render graph on the message
        thread and waits for it, so a render driven from a blocked message thread
        would never finish.
    */
    bool renderOffTheMessageThread (const std::function<bool()>& render, bool& offTheMessageThread)
    {
        std::atomic<bool> finished { false };
        bool rendered = false;
        bool elsewhere = false;

        std::thread worker { [&]
                             {
                                 elsewhere = ! juce::MessageManager::existsAndIsCurrentThread();
                                 rendered = render();
                                 finished = true;
                             } };

        while (! finished)
            pumpMessages (10);

        worker.join();
        offTheMessageThread = elsewhere;

        return rendered;
    }

    /** A file of this folder that no render has written before. */
    std::filesystem::path freshDestination (const std::filesystem::path& folder)
    {
        static std::atomic<int> counter { 0 };

        return folder / ("render-" + std::to_string (counter++) + ".wav");
    }
} // namespace

//==============================================================================
Render::Render (std::filesystem::path audioFile) : audio (std::move (audioFile))
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    const std::unique_ptr<juce::AudioFormatReader> reader { formats.createReaderFor (
        toJuceFile (audio)) };

    if (reader == nullptr || reader->lengthInSamples == 0 || reader->sampleRate <= 0.0
        || reader->numChannels == 0)
        return;

    sampleRate = reader->sampleRate;

    juce::AudioBuffer<float> buffer { static_cast<int> (reader->numChannels),
                                      static_cast<int> (reader->lengthInSamples) };
    reader->read (&buffer, 0, buffer.getNumSamples(), 0, true, true);

    channels.reserve (static_cast<std::size_t> (buffer.getNumChannels()));

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const std::span samples { buffer.getReadPointer (channel),
                                  static_cast<std::size_t> (buffer.getNumSamples()) };
        channels.emplace_back (samples.begin(), samples.end());
    }
}

double Render::lengthSeconds() const
{
    return readable() ? static_cast<double> (channels.front().size()) / sampleRate : 0.0;
}

namespace
{
    /** The half-open stretch of samples a stretch of seconds names, clipped to
        what the render holds.
    */
    std::pair<std::size_t, std::size_t>
        samplesBetween (std::size_t length, double sampleRate, double fromSeconds, double toSeconds)
    {
        const auto toSample = [&] (double seconds)
        {
            if (! (seconds > 0.0))
                return std::size_t { 0 };

            const auto sample = seconds * sampleRate;

            return sample >= static_cast<double> (length) ? length
                                                          : static_cast<std::size_t> (sample);
        };

        const auto first = toSample (fromSeconds);
        const auto last = toSample (toSeconds);

        return { first, std::max (first, last) };
    }
} // namespace

double Render::peakBetween (double fromSeconds, double toSeconds) const
{
    if (! readable())
        return 0.0;

    const auto [first, last] =
        samplesBetween (channels.front().size(), sampleRate, fromSeconds, toSeconds);
    double peak = 0.0;

    for (const auto& channel : channels)
        for (auto sample = first; sample < last; ++sample)
            peak = std::max (peak, std::abs (static_cast<double> (channel[sample])));

    return peak;
}

double Render::rmsBetween (double fromSeconds, double toSeconds) const
{
    if (! readable())
        return 0.0;

    const auto [first, last] =
        samplesBetween (channels.front().size(), sampleRate, fromSeconds, toSeconds);

    if (first == last)
        return 0.0;

    double sumOfSquares = 0.0;

    for (const auto& channel : channels)
        for (auto sample = first; sample < last; ++sample)
            sumOfSquares += static_cast<double> (channel[sample]) * channel[sample];

    return std::sqrt (sumOfSquares / static_cast<double> ((last - first) * channels.size()));
}

bool Render::isSilentBetween (double fromSeconds, double toSeconds) const
{
    return rmsBetween (fromSeconds, toSeconds) < silenceLevel;
}

double Render::pitchHzBetween (double fromSeconds, double toSeconds) const
{
    if (! readable())
        return 0.0;

    const auto& samples = channels.front();
    const auto [first, last] = samplesBetween (samples.size(), sampleRate, fromSeconds, toSeconds);

    std::size_t firstCrossing = 0;
    std::size_t lastCrossing = 0;
    int crossings = 0;

    for (auto sample = std::max (first, std::size_t { 1 }); sample < last; ++sample)
    {
        if (samples[sample - 1] <= 0.0F && samples[sample] > 0.0F)
        {
            if (crossings == 0)
                firstCrossing = sample;

            lastCrossing = sample;
            ++crossings;
        }
    }

    if (crossings < fewestCrossings)
        return 0.0;

    return static_cast<double> (crossings - 1) * sampleRate
           / static_cast<double> (lastCrossing - firstCrossing);
}

std::vector<double> Render::onsetsSeconds() const
{
    std::vector<double> onsets;

    if (! readable())
        return onsets;

    const auto levelAt = [this] (std::size_t sample)
    {
        double level = 0.0;

        for (const auto& channel : channels)
            level = std::max (level, std::abs (static_cast<double> (channel[sample])));

        return level;
    };

    // Where a sound began, given the sample it first grew loud enough to notice
    // at: the last sample in front of it that carried nothing.
    //
    // Every sound rises out of silence over a few milliseconds, so answering
    // with the moment it crossed a level would call every one of them late, and
    // an onset that reads late is the one thing this measurement may not do.
    // The walk back looks at a few milliseconds at a time rather than one
    // sample, because a tone passes through zero twice a cycle and a single
    // sample of it reads exactly like the silence in front of it. It gives up
    // after one render block, which is the whole tolerance an onset has anyway.
    const auto carriedSomethingBefore = [&] (std::size_t sample)
    {
        const auto envelopeSamples = static_cast<std::size_t> (onsetEnvelopeSeconds * sampleRate);
        const auto from = sample > envelopeSamples ? sample - envelopeSamples : std::size_t { 0 };

        for (auto earlier = from; earlier < sample; ++earlier)
            if (levelAt (earlier) > silenceLevel)
                return true;

        return false;
    };

    const auto soundBeganAt = [&] (std::size_t sample)
    {
        const auto blockSamples = static_cast<std::size_t> (renderBlockSize);
        const auto earliest = sample > blockSamples ? sample - blockSamples : std::size_t { 0 };

        while (sample > earliest && carriedSomethingBefore (sample))
            --sample;

        return sample > 0 ? sample - 1 : sample;
    };

    const auto quietSamples = static_cast<std::size_t> (onsetQuietSeconds * sampleRate);
    auto quietFor = quietSamples;

    for (std::size_t sample = 0; sample < channels.front().size(); ++sample)
    {
        const auto level = levelAt (sample);

        if (level > onsetLevel)
        {
            if (quietFor >= quietSamples)
                onsets.push_back (static_cast<double> (soundBeganAt (sample)) / sampleRate);

            quietFor = 0;
        }
        else
        {
            ++quietFor;
        }
    }

    return onsets;
}

double Render::levelChangeDb (double firstFromSeconds,
                              double firstToSeconds,
                              double secondFromSeconds,
                              double secondToSeconds) const
{
    const auto before = rmsBetween (firstFromSeconds, firstToSeconds);
    const auto after = rmsBetween (secondFromSeconds, secondToSeconds);

    if (before <= 0.0)
        return after > 0.0 ? -silentDb : 0.0;

    return toDecibels (after / before);
}

double Render::toneLevelDbBetween (double frequencyHz, double fromSeconds, double toSeconds) const
{
    if (! readable() || frequencyHz <= 0.0 || frequencyHz >= sampleRate / 2.0)
        return silentDb;

    const auto& samples = channels.front();
    const auto [first, last] = samplesBetween (samples.size(), sampleRate, fromSeconds, toSeconds);
    const auto count = last - first;

    if (count == 0)
        return silentDb;

    // Goertzel: one bin of the spectrum, which is all a test that names a
    // frequency is asking about, and no dependency to hold one frequency up
    // against the rest of the render.
    const auto coefficient = 2.0 * std::cos (2.0 * std::numbers::pi * frequencyHz / sampleRate);
    double previous = 0.0;
    double beforeThat = 0.0;

    for (auto sample = first; sample < last; ++sample)
    {
        const auto current =
            static_cast<double> (samples[sample]) + coefficient * previous - beforeThat;
        beforeThat = previous;
        previous = current;
    }

    const auto power =
        previous * previous + beforeThat * beforeThat - coefficient * previous * beforeThat;

    return toDecibels (2.0 * std::sqrt (std::max (0.0, power)) / static_cast<double> (count));
}

bool Render::isBitIdenticalTo (const Render& other) const
{
    if (! readable() || ! other.readable() || channels.size() != other.channels.size()
        || ! juce::exactlyEqual (sampleRate, other.sampleRate))
        return false;

    for (std::size_t channel = 0; channel < channels.size(); ++channel)
    {
        const auto& ours = channels[channel];
        const auto& theirs = other.channels[channel];

        if (ours.size() != theirs.size()
            || std::memcmp (ours.data(), theirs.data(), ours.size() * sizeof (float)) != 0)
            return false;
    }

    return true;
}

//==============================================================================
Render renderProject (duet::model::Session& session, const std::filesystem::path& folder)
{
    const auto destination = freshDestination (folder);
    bool offTheMessageThread = false;

    if (! renderOffTheMessageThread ([&] { return session.renderToFile (destination); },
                                     offTheMessageThread))
        return Render { {} };

    Render render { destination };
    render.offTheMessageThread = offTheMessageThread;

    return render;
}

Render renderTrack (duet::model::Session& session,
                    duet::model::TrackRef track,
                    const std::filesystem::path& folder)
{
    const auto destination = freshDestination (folder);
    bool offTheMessageThread = false;

    if (! renderOffTheMessageThread ([&] { return session.renderTrackToFile (track, destination); },
                                     offTheMessageThread))
        return Render { {} };

    Render render { destination };
    render.offTheMessageThread = offTheMessageThread;

    return render;
}
} // namespace duet::testing
