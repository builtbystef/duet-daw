// PROTOTYPE — disposable implementation for roadmap node xciphe.

#include "OfflineRenderPrototype.h"

#include <tracktion_engine/tracktion_engine.h>

#include <bit>
#include <cstring>

namespace duet::prototype
{

namespace te = tracktion;

namespace
{

constexpr auto sampleRate = 44100.0;
constexpr auto durationSeconds = 2.0;

class HeadlessEngineBehaviour final : public te::EngineBehaviour
{
public:
    bool autoInitialiseDeviceManager() override { return false; }
    bool addSystemAudioIODeviceTypes() override { return false; }
};

void writeKnownTone (const juce::File& file)
{
    constexpr auto frequencyHz = 440.0;
    constexpr auto amplitude = 0.5f;
    const auto numSamples = static_cast<int> (sampleRate * durationSeconds);

    juce::AudioBuffer<float> source (2, numSamples);
    for (auto sample = 0; sample < numSamples; ++sample)
    {
        const auto phase = juce::MathConstants<double>::twoPi * frequencyHz
                           * static_cast<double> (sample) / sampleRate;
        const auto value = amplitude * static_cast<float> (std::sin (phase));
        source.setSample (0, sample, value);
        source.setSample (1, sample, value);
    }

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream =
        std::make_unique<juce::FileOutputStream> (file);
    const auto options = juce::AudioFormatWriterOptions {}
                             .withSampleRate (sampleRate)
                             .withNumChannels (2)
                             .withBitsPerSample (32);
    auto writer = wav.createWriterFor (stream, options);
    jassert (writer != nullptr);
    writer->writeFromAudioSampleBuffer (source, 0, source.getNumSamples());
}

RenderResult loadRender (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    auto reader = std::unique_ptr<juce::AudioFormatReader> (formats.createReaderFor (file));
    if (reader == nullptr)
        return {};

    RenderResult result;
    result.sampleRate = reader->sampleRate;
    result.audio.setSize (static_cast<int> (reader->numChannels),
                          static_cast<int> (reader->lengthInSamples));
    if (! reader->read (&result.audio, 0, result.audio.getNumSamples(), 0, true, true))
        return {};

    return result;
}

RenderResult renderEdit (te::Engine& engine, te::Edit& edit, double lengthSeconds)
{
    juce::TemporaryFile renderFile (".wav");
    te::Renderer::Parameters parameters (edit);
    parameters.destFile = renderFile.getFile();
    parameters.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
    parameters.bitDepth = 32;
    parameters.sampleRateForAudio = sampleRate;
    parameters.blockSizeForAudio = 512;
    parameters.time = { te::TimePosition(),
                        te::TimePosition::fromSeconds (lengthSeconds) };
    parameters.tracksToDo = te::toBitSet (te::getAllTracks (edit));
    parameters.canRenderInMono = false;
    parameters.ditheringEnabled = false;

    te::Renderer::RenderTask renderTask { "Offline render prototype", parameters,
                                          nullptr, nullptr };
    while (renderTask.runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain)
    {
    }

    return loadRender (renderFile.getFile());
}

struct KnownToneProject
{
    KnownToneProject()
    {
        writeKnownTone (sourceFile.getFile());

        edit = te::createEmptyEdit (engine, editFile.getFile());
        edit->ensureNumberOfAudioTracks (1);
        auto* track = te::getAudioTracks (*edit)[0];
        const te::ClipPosition position {
            { te::TimePosition(), te::TimePosition::fromSeconds (durationSeconds) }
        };
        auto clip = track->insertWaveClip ("known 440 Hz tone", sourceFile.getFile(),
                                           position, false);
        jassert (clip != nullptr);
        clip->getSourceFileReference().setToFile (
            sourceFile.getFile(), te::SourceFileReference::PathStyle::alwaysAbsolute, false);
    }

    RenderResult render()
    {
        return renderEdit (engine, *edit, durationSeconds);
    }

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    te::Engine engine { "DuetOfflineRenderPrototype", nullptr,
                        std::make_unique<HeadlessEngineBehaviour>() };
    juce::TemporaryFile sourceFile { ".wav" };
    juce::TemporaryFile editFile { te::editFileSuffix };
    std::unique_ptr<te::Edit> edit;
};

class StandInInstrumentProcessor final : public juce::AudioProcessor
{
public:
    StandInInstrumentProcessor()
        : juce::AudioProcessor (BusesProperties().withOutput (
              "Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    const juce::String getName() const override { return "Stand-in instrument"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void prepareToPlay (double newSampleRate, int) override
    {
        processorSampleRate = newSampleRate;
        phase = 0.0;
        gate = false;
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer,
                       juce::MidiBuffer& midi) override
    {
        buffer.clear();
        auto event = midi.begin();
        const auto end = midi.end();

        for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            while (event != end && (*event).samplePosition <= sample)
            {
                const auto message = (*event).getMessage();
                if (message.isNoteOn())
                {
                    currentNote = message.getNoteNumber();
                    frequencyHz = juce::MidiMessage::getMidiNoteInHertz (currentNote);
                    phase = 0.0;
                    gate = true;
                }
                else if (message.isNoteOff() && message.getNoteNumber() == currentNote)
                {
                    gate = false;
                }
                ++event;
            }

            const auto value = gate ? 0.4f * static_cast<float> (std::sin (phase)) : 0.0f;
            if (gate)
            {
                phase += juce::MathConstants<double>::twoPi * frequencyHz
                         / processorSampleRate;
                if (phase >= juce::MathConstants<double>::twoPi)
                    phase -= juce::MathConstants<double>::twoPi;
            }

            for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.setSample (channel, sample, value);
        }
    }

    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    double processorSampleRate = sampleRate;
    double frequencyHz = 0.0;
    double phase = 0.0;
    int currentNote = -1;
    bool gate = false;
};

class StandInInstrumentPlugin final : public te::Plugin
{
public:
    static constexpr const char* xmlTypeName = "duetPrototypeInstrument";

    explicit StandInInstrumentPlugin (te::PluginCreationInfo info)
        : te::Plugin (info)
    {
    }

    ~StandInInstrumentPlugin() override { notifyListenersOfDeletion(); }

    juce::String getName() const override { return "Duet prototype instrument"; }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    int getNumOutputChannelsGivenInputs (int) override { return 2; }
    BusLayout getBusses() const override
    {
        BusLayout layout;
        layout.outputs.push_back (te::ChannelConfiguration::stereo());
        return layout;
    }

    bool takesMidiInput() override { return true; }
    bool isSynth() override { return true; }
    bool producesAudioWhenNoAudioInput() override { return true; }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        processor.prepareToPlay (info.sampleRate, info.blockSizeSamples);
    }

    void deinitialise() override { processor.releaseResources(); }
    void reset() override { processor.reset(); }

    void applyToBuffer (const te::PluginRenderContext& context) override
    {
        if (context.destBuffer == nullptr)
            return;

        juce::AudioBuffer<float> block (
            context.destBuffer->getArrayOfWritePointers(),
            context.destBuffer->getNumChannels(), context.bufferStartSample,
            context.bufferNumSamples);
        juce::MidiBuffer midi;

        if (context.bufferForMidiMessages != nullptr)
        {
            for (const auto& message : *context.bufferForMidiMessages)
            {
                const auto absoluteSample = juce::roundToInt (
                    message.getTimeStamp() * processor.getSampleRate());
                if (absoluteSample >= context.bufferStartSample
                    && absoluteSample < context.bufferStartSample + context.bufferNumSamples)
                    midi.addEvent (message, absoluteSample - context.bufferStartSample);
            }
        }

        processor.processBlock (block, midi);
    }

    void restorePluginStateFromValueTree (const juce::ValueTree&) override {}

private:
    StandInInstrumentProcessor processor;
};

class StandInGainProcessor final : public juce::AudioProcessor
{
public:
    StandInGainProcessor()
        : juce::AudioProcessor (
              BusesProperties()
                  .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                  .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    const juce::String getName() const override { return "Stand-in gain"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        buffer.applyGain (0.5f);
    }

    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
};

class StandInGainPlugin final : public te::Plugin
{
public:
    static constexpr const char* xmlTypeName = "duetPrototypeGain";

    explicit StandInGainPlugin (te::PluginCreationInfo info)
        : te::Plugin (info)
    {
    }

    ~StandInGainPlugin() override { notifyListenersOfDeletion(); }

    juce::String getName() const override { return "Duet prototype gain"; }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }
    BusLayout getBusses() const override { return BusLayout::singleStereoInOut(); }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        processor.prepareToPlay (info.sampleRate, info.blockSizeSamples);
    }

    void deinitialise() override { processor.releaseResources(); }
    void reset() override { processor.reset(); }

    void applyToBuffer (const te::PluginRenderContext& context) override
    {
        if (context.destBuffer == nullptr)
            return;

        juce::AudioBuffer<float> block (
            context.destBuffer->getArrayOfWritePointers(),
            context.destBuffer->getNumChannels(), context.bufferStartSample,
            context.bufferNumSamples);
        juce::MidiBuffer midi;
        processor.processBlock (block, midi);
    }

    void restorePluginStateFromValueTree (const juce::ValueTree&) override {}

private:
    StandInGainProcessor processor;
};

double estimatePitchFromRisingCrossings (const RenderResult& render,
                                         double startSeconds,
                                         double endSeconds)
{
    const auto start = static_cast<int> (startSeconds * render.sampleRate);
    const auto end = static_cast<int> (endSeconds * render.sampleRate);
    const auto* samples = render.audio.getReadPointer (0);
    int firstCrossing = -1;
    int lastCrossing = -1;
    int crossings = 0;

    for (auto sample = start + 1; sample < end; ++sample)
    {
        if (samples[sample - 1] <= 0.0f && samples[sample] > 0.0f)
        {
            if (firstCrossing < 0)
                firstCrossing = sample;
            lastCrossing = sample;
            ++crossings;
        }
    }

    if (crossings < 2)
        return 0.0;
    return static_cast<double> (crossings - 1) * render.sampleRate
           / static_cast<double> (lastCrossing - firstCrossing);
}

std::vector<double> findOnsets (const RenderResult& render)
{
    std::vector<double> onsets;
    const auto* samples = render.audio.getReadPointer (0);
    const auto minimumSilence = static_cast<int> (render.sampleRate * 0.1);
    auto silentSamples = minimumSilence;

    for (auto sample = 0; sample < render.audio.getNumSamples(); ++sample)
    {
        if (std::abs (samples[sample]) > 0.01f)
        {
            if (silentSamples >= minimumSilence)
                onsets.push_back (static_cast<double> (sample) / render.sampleRate);
            silentSamples = 0;
        }
        else
        {
            ++silentSamples;
        }
    }

    return onsets;
}

} // namespace

RenderResult renderKnownToneEdit()
{
    KnownToneProject project;
    return project.render();
}

DeterminismEvidence measureKnownToneDeterminism()
{
    KnownToneProject project;
    const auto first = project.render();
    const auto second = project.render();

    DeterminismEvidence evidence;
    if (first.sampleRate != second.sampleRate
        || first.audio.getNumChannels() != second.audio.getNumChannels()
        || first.audio.getNumSamples() != second.audio.getNumSamples())
        return evidence;

    evidence.bitExact = true;
    evidence.firstRenderFingerprint = 14695981039346656037ULL;
    for (auto channel = 0; channel < first.audio.getNumChannels(); ++channel)
    {
        const auto* a = first.audio.getReadPointer (channel);
        const auto* b = second.audio.getReadPointer (channel);
        evidence.bitExact = evidence.bitExact
                            && std::memcmp (a, b,
                                            static_cast<std::size_t> (first.audio.getNumSamples())
                                                * sizeof (float)) == 0;

        for (auto sample = 0; sample < first.audio.getNumSamples(); ++sample)
        {
            const auto bits = std::bit_cast<std::uint32_t> (a[sample]);
            for (auto byte = 0; byte < 4; ++byte)
            {
                evidence.firstRenderFingerprint ^=
                    static_cast<std::uint8_t> (bits >> (byte * 8));
                evidence.firstRenderFingerprint *= 1099511628211ULL;
            }

            const auto difference = std::abs (a[sample] - b[sample]);
            evidence.maximumAbsoluteDifference =
                std::max (evidence.maximumAbsoluteDifference, difference);
            evidence.differingSamples += difference != 0.0f ? 1 : 0;
        }
    }

    return evidence;
}

InstrumentEvidence measureStandInInstrumentFeatures()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    te::Engine engine { "DuetInstrumentFeaturePrototype", nullptr,
                        std::make_unique<HeadlessEngineBehaviour>() };
    engine.getPluginManager().createBuiltInType<StandInInstrumentPlugin>();

    juce::TemporaryFile editFile (te::editFileSuffix);
    auto edit = te::createEmptyEdit (engine, editFile.getFile());
    edit->ensureNumberOfAudioTracks (1);
    auto* track = te::getAudioTracks (*edit)[0];

    auto midiClip = track->insertMIDIClip (
        { te::TimePosition(), te::TimePosition::fromSeconds (durationSeconds) }, nullptr);
    if (midiClip == nullptr)
        return {};

    auto& sequence = midiClip->getSequence();
    sequence.addNote (69, te::BeatPosition::fromBeats (0.5),
                      te::BeatDuration::fromBeats (0.5), 100, 0, nullptr);
    sequence.addNote (72, te::BeatPosition::fromBeats (2.0),
                      te::BeatDuration::fromBeats (0.5), 100, 0, nullptr);

    auto instrument = edit->getPluginCache().createNewPlugin (
        StandInInstrumentPlugin::xmlTypeName, {});
    track->pluginList.insertPlugin (instrument, 0, nullptr);

    const auto rendered = renderEdit (engine, *edit, durationSeconds);
    const auto onsets = findOnsets (rendered);

    InstrumentEvidence evidence;
    evidence.firstPitchHz = estimatePitchFromRisingCrossings (rendered, 0.30, 0.45);
    evidence.secondPitchHz = estimatePitchFromRisingCrossings (rendered, 1.05, 1.20);
    if (! onsets.empty())
        evidence.firstOnsetSeconds = onsets[0];
    if (onsets.size() > 1)
        evidence.secondOnsetSeconds = onsets[1];
    return evidence;
}

EffectEvidence measureStandInEffectFeatures()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    te::Engine engine { "DuetEffectFeaturePrototype", nullptr,
                        std::make_unique<HeadlessEngineBehaviour>() };
    engine.getPluginManager().createBuiltInType<StandInGainPlugin>();

    juce::TemporaryFile sourceFile (".wav");
    juce::TemporaryFile editFile (te::editFileSuffix);
    writeKnownTone (sourceFile.getFile());

    auto edit = te::createEmptyEdit (engine, editFile.getFile());
    edit->ensureNumberOfAudioTracks (1);
    auto* track = te::getAudioTracks (*edit)[0];
    const te::ClipPosition position {
        { te::TimePosition(), te::TimePosition::fromSeconds (durationSeconds) }
    };
    auto clip = track->insertWaveClip ("known 440 Hz tone", sourceFile.getFile(),
                                       position, false);
    if (clip == nullptr)
        return {};
    clip->getSourceFileReference().setToFile (
        sourceFile.getFile(), te::SourceFileReference::PathStyle::alwaysAbsolute, false);

    const auto dry = renderEdit (engine, *edit, durationSeconds);
    auto gain = edit->getPluginCache().createNewPlugin (StandInGainPlugin::xmlTypeName, {});
    track->pluginList.insertPlugin (gain, 0, nullptr);
    const auto wet = renderEdit (engine, *edit, durationSeconds);

    EffectEvidence evidence;
    evidence.dryRms = dry.audio.getRMSLevel (0, 0, dry.audio.getNumSamples());
    evidence.wetRms = wet.audio.getRMSLevel (0, 0, wet.audio.getNumSamples());
    if (evidence.dryRms > 0.0f && evidence.wetRms > 0.0f)
        evidence.levelChangeDb = juce::Decibels::gainToDecibels (
            static_cast<double> (evidence.wetRms / evidence.dryRms));
    return evidence;
}

} // namespace duet::prototype
