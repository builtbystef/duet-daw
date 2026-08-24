#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>

namespace
{
class GoodProcessor final : public juce::AudioProcessor
{
public:
    using juce::AudioProcessor::processBlock;

    GoodProcessor()
        : AudioProcessor (BusesProperties()
                              .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
        gain = new juce::AudioParameterFloat (
            juce::ParameterID { "gain", 1 }, "Gain", 0.0F, 1.0F, 1.0F);
        addParameter (gain);
    }

    [[nodiscard]] const juce::String getName() const override { return "Duet Good VST3 Fixture"; }
    void prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/) override {}
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& /*midi*/) override
    {
        audio.applyGain (gain->get());
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

    void getStateInformation (juce::MemoryBlock& destination) override
    {
        juce::MemoryOutputStream { destination, false }.writeFloat (gain->get());
    }

    void setStateInformation (const void* data, int size) override
    {
        if (data == nullptr || size < static_cast<int> (sizeof (float)))
            return;

        juce::MemoryInputStream input { data, static_cast<std::size_t> (size), false };
        *gain = std::clamp (input.readFloat(), 0.0F, 1.0F);
    }

private:
    juce::AudioParameterFloat* gain = nullptr;
};
} // namespace

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GoodProcessor; }
