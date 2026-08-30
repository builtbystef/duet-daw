#include <juce_audio_processors/juce_audio_processors.h>

#include <dlfcn.h>

#include <atomic>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace
{
constexpr int moduleAnchor = 0;

/** The file whose presence beside this bundle turns the parameter below
    hostile.

    A plugin that raised the moment it was scanned would never be hosted, and
    what is being asserted is what happens to a plugin the producer already has
    in a chain. So the fixture is well behaved until a test says otherwise, and
    the marker file is how a test says it: dropped into the bundle once the
    plugin is loaded and playing.
*/
std::filesystem::path markerBesideThisBundle()
{
    Dl_info module {};

    if (dladdr (&moduleAnchor, &module) == 0 || module.dli_fname == nullptr)
        return {};

    const auto binary = std::filesystem::path { module.dli_fname };

    return binary.parent_path().parent_path().parent_path() / "raise-on-read.txt";
}

/** A parameter that answers its value happily and raises when asked what that
    value means, which is the one thing about a hosted plugin Duet has to ask
    the plugin itself.
*/
class RaisingParameter final : public juce::AudioProcessorParameter
{
public:
    explicit RaisingParameter (std::filesystem::path marker) : hostile (std::move (marker)) {}

    [[nodiscard]] float getValue() const override { return value; }
    void setValue (float newValue) override { value = newValue; }
    [[nodiscard]] float getDefaultValue() const override { return 1.0F; }
    [[nodiscard]] juce::String getLabel() const override { return {}; }

    [[nodiscard]] juce::String getName (int maximumStringLength) const override
    {
        return juce::String { "Gain" }.substring (0, maximumStringLength);
    }

    [[nodiscard]] juce::String getText (float raw, int /*maximumStringLength*/) const override
    {
        if (! hostile.empty() && std::filesystem::exists (hostile))
            throw std::runtime_error ("the fixture was asked what its value means");

        return juce::String { raw, 2 };
    }

    [[nodiscard]] float getValueForText (const juce::String& text) const override
    {
        return text.getFloatValue();
    }

private:
    std::filesystem::path hostile;
    std::atomic<float> value { 1.0F };
};

class RaisingProcessor final : public juce::AudioProcessor
{
public:
    using juce::AudioProcessor::processBlock;

    RaisingProcessor()
        : AudioProcessor (BusesProperties()
                              .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
        gain = new RaisingParameter (markerBesideThisBundle());
        addParameter (gain);
    }

    [[nodiscard]] const juce::String getName() const override
    {
        return "Duet Raising VST3 Fixture";
    }

    void prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/) override {}
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& /*midi*/) override
    {
        audio.applyGain (gain->getValue());
    }

    [[nodiscard]] double getTailLengthSeconds() const override { return 0.0; }
    [[nodiscard]] bool acceptsMidi() const override { return false; }
    [[nodiscard]] bool producesMidi() const override { return false; }
    [[nodiscard]] juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    [[nodiscard]] bool hasEditor() const override { return false; }
    [[nodiscard]] int getNumPrograms() override { return 1; }
    [[nodiscard]] int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int /*index*/) override {}
    [[nodiscard]] const juce::String getProgramName (int /*index*/) override { return {}; }
    void changeProgramName (int /*index*/, const juce::String& /*name*/) override {}
    void getStateInformation (juce::MemoryBlock& /*destination*/) override {}
    void setStateInformation (const void* /*data*/, int /*size*/) override {}

private:
    RaisingParameter* gain = nullptr;
};
} // namespace

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RaisingProcessor; }
