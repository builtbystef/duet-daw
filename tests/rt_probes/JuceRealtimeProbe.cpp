#include <duet/realtime/Callback.h>

#include <juce_audio_processors/juce_audio_processors.h>

/** The JUCE-hosted probe: a `juce::AudioProcessor`, entered at `processBlock`.

    It is a VST3 because that is the only way the engine hosts a JUCE processor
    (milestone one hosts VST3 through the engine's `ExternalPlugin`, and nothing else), so the probe is
    scanned and inserted exactly as a producer's plugin would be, and the
    callback the engine reaches is `processBlock` with nothing of Duet's above
    it — which is what makes the annotation on it the entry to the real-time
    context rather than a second one inside somebody else's.

    The plugin's own instrumentation lives in this shared object and calls the
    sanitizer runtime in the test executable that loaded it, so the report names
    this callback even though the bundle is opened at run time.
*/
namespace
{
class RealtimeProbeProcessor final : public juce::AudioProcessor
{
public:
    using juce::AudioProcessor::processBlock;

    RealtimeProbeProcessor()
        : AudioProcessor (BusesProperties()
                              .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    [[nodiscard]] const juce::String getName() const override { return "Duet Realtime Probe VST3"; }

    void prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/) override {}
    void releaseResources() override {}

    /** The callback. Half the level it was given, over the buffer the host
        owns: bounded, allocation-free, and the smallest thing that proves from
        outside that it ran.

        The half is `duet::testing::realtimeProbeGain`, written out rather than
        included: a VST3 bundle is its own binary and linking the model into one
        to reach a constant would be a worse trade than repeating it.
    */
    void processBlock (juce::AudioBuffer<float>& audio,
                       juce::MidiBuffer& /*midi*/) noexcept DUET_NONBLOCKING override
    {
        audio.applyGain (0.5F);
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
};
} // namespace

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RealtimeProbeProcessor; }
