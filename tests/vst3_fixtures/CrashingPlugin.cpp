#include <juce_audio_processors/juce_audio_processors.h>

#include <dlfcn.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace
{
constexpr int moduleAnchor = 0;

class CrashingProcessor final : public juce::AudioProcessor
{
public:
    using juce::AudioProcessor::processBlock;

    CrashingProcessor()
        : AudioProcessor (BusesProperties()
                              .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
        Dl_info module {};

        if (dladdr (&moduleAnchor, &module) != 0 && module.dli_fname != nullptr)
        {
            const auto binary = std::filesystem::path { module.dli_fname };
            const auto bundle = binary.parent_path().parent_path().parent_path();
            std::ofstream { bundle / "crash-loads.txt", std::ios::app } << "loaded\n";
        }

        std::abort();
    }

    [[nodiscard]] const juce::String getName() const override
    {
        return "Duet Crashing VST3 Fixture";
    }

    void prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>& /*audio*/, juce::MidiBuffer& /*midi*/) override {}
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
};
} // namespace

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CrashingProcessor; }
