#include <duet/testing/TestSupport.h>

#include <duet/testing/RenderHarness.h>

#include <limits>

#include <duet/model/Session.h>
#include <duet/persistence/ProjectLayout.h>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <numbers>

namespace duet::testing
{
namespace
{
    constexpr int playAttempts = 20;
    constexpr int msPerPlayAttempt = 200;

    /** How much message loop one look of pumpUntil is worth: short enough that
        a condition that holds early is seen early, long enough that a timer on
        a millisecond gets its tick.
    */
    constexpr int msPerPumpUntilLook = 5;

    constexpr double toneSampleRate = 44100.0;
    constexpr int toneBitDepth = 16;
    constexpr float toneAmplitude = 0.5F;

    juce::File toJuceFile (const std::filesystem::path& path)
    {
        return juce::File { juce::String { path.string() } };
    }
} // namespace

TempProject::TempProject()
{
    static std::atomic<int> counter { 0 };

    projectFolder = std::filesystem::temp_directory_path()
                    / ("duet-tests-" + std::to_string (counter++) + "-"
                       + std::to_string (juce::Random::getSystemRandom().nextInt64()));

    std::filesystem::create_directories (duet::persistence::audioDirectory (projectFolder));
}

TempProject::~TempProject()
{
    std::error_code ignored;
    std::filesystem::remove_all (projectFolder, ignored);
}

std::filesystem::path TempProject::editFile() const
{
    return duet::persistence::editFile (projectFolder);
}

std::filesystem::path TempProject::writeTone (std::string_view fileName,
                                              double lengthSeconds,
                                              double frequencyHz) const
{
    const auto file = duet::persistence::audioDirectory (projectFolder) / fileName;
    const auto numSamples = static_cast<int> (toneSampleRate * lengthSeconds);

    juce::AudioBuffer<float> buffer { 2, numSamples };

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto phase = 2.0 * std::numbers::pi * frequencyHz * sample / toneSampleRate;
        const auto value = static_cast<float> (std::sin (phase)) * toneAmplitude;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample (channel, sample, value);
    }

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream { std::make_unique<juce::FileOutputStream> (
        toJuceFile (file)) };
    const auto writer = wav.createWriterFor (stream,
                                             juce::AudioFormatWriterOptions {}
                                                 .withSampleRate (toneSampleRate)
                                                 .withNumChannels (buffer.getNumChannels())
                                                 .withBitsPerSample (toneBitDepth));

    if (writer != nullptr)
        writer->writeFromAudioSampleBuffer (buffer, 0, numSamples);

    return file;
}

void pumpMessages (int milliseconds)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds);
}

bool pumpUntil (const std::function<bool()>& condition, int timeoutMilliseconds)
{
    const auto startedAt = juce::Time::getMillisecondCounter();
    const auto bound = static_cast<juce::uint32> (std::max (0, timeoutMilliseconds));

    while (! condition())
    {
        // Unsigned throughout, so the counter's wrap is a difference like any
        // other rather than a wait that never ends.
        if (juce::Time::getMillisecondCounter() - startedAt >= bound)
            return false;

        pumpMessages (msPerPumpUntilLook);
    }

    return true;
}

bool playUntilRolling (duet::model::Session& session)
{
    session.startPlayback();

    for (int attempt = 0; attempt < playAttempts && ! session.isPlaying(); ++attempt)
        pumpMessages (msPerPlayAttempt);

    return session.isPlaying();
}

double peakLevelOf (const std::filesystem::path& audioFile)
{
    return peakLevelBetween (audioFile, 0.0, std::numeric_limits<double>::max());
}

double
    peakLevelBetween (const std::filesystem::path& audioFile, double fromSeconds, double toSeconds)
{
    return Render { audioFile }.peakBetween (fromSeconds, toSeconds);
}
} // namespace duet::testing
