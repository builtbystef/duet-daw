#include <duet/model/AudioFile.h>

#include <juce_audio_formats/juce_audio_formats.h>

#include <memory>
#include <span>

namespace duet::model
{
AudioSamples readAudioFile (const std::filesystem::path& file)
{
    AudioSamples samples;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    const std::unique_ptr<juce::AudioFormatReader> reader { formats.createReaderFor (
        juce::File { juce::String { file.string() } }) };

    if (reader == nullptr || reader->lengthInSamples == 0 || reader->sampleRate <= 0.0
        || reader->numChannels == 0)
        return samples;

    juce::AudioBuffer<float> buffer { static_cast<int> (reader->numChannels),
                                      static_cast<int> (reader->lengthInSamples) };
    reader->read (&buffer, 0, buffer.getNumSamples(), 0, true, true);

    samples.sampleRate = reader->sampleRate;
    samples.channels.reserve (static_cast<std::size_t> (buffer.getNumChannels()));

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const std::span read { buffer.getReadPointer (channel),
                               static_cast<std::size_t> (buffer.getNumSamples()) };
        samples.channels.emplace_back (read.begin(), read.end());
    }

    return samples;
}
} // namespace duet::model
