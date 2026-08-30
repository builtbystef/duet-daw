#include "SessionImpl.h"

#include <algorithm>

/** The machine's audio and MIDI hardware, as the Settings window's Audio and
    MIDI tabs set it.

    None of it is the project's. Which device the producer hears, at what rate
    and with what buffer, and which MIDI inputs are switched on, are facts about
    their machine that outlive every project — so nothing here is written into
    the project, its undo history, or its dirty state. The engine keeps them in
    the app-global store the session lends it, which is what makes a choice made
    here the choice on the next launch.
*/
namespace duet::model
{
namespace
{
    /** The driver the machine is on, or nothing when the engine has none —
        which is what a machine with no audio hardware looks like, and what a
        session that gave its device up looks like.
    */
    juce::AudioIODeviceType* currentDeviceType (const juce::AudioDeviceManager& devices)
    {
        return devices.getCurrentDeviceTypeObject();
    }

    std::vector<std::string> deviceNames (const juce::AudioDeviceManager& devices, bool wantInputs)
    {
        std::vector<std::string> names;
        auto* type = currentDeviceType (devices);

        if (type == nullptr)
            return names;

        // The list the driver keeps is only as fresh as the last time it was
        // asked to look, and a producer opening the tab has had time to plug
        // something in.
        type->scanForDevices();

        for (const auto& name : type->getDeviceNames (wantInputs))
            names.push_back (name.toStdString());

        return names;
    }

    double latencyMs (int samples, double sampleRate)
    {
        return sampleRate > 0.0 ? 1000.0 * samples / sampleRate : 0.0;
    }
} // namespace

//==============================================================================
std::vector<std::string> Session::availableOutputDevices() const
{
    return deviceNames (impl->engine.getDeviceManager().deviceManager, false);
}

std::vector<std::string> Session::availableInputDevices() const
{
    return deviceNames (impl->engine.getDeviceManager().deviceManager, true);
}

std::vector<double> Session::availableSampleRates() const
{
    std::vector<double> rates;
    auto* device = impl->engine.getDeviceManager().deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
        return rates;

    for (const auto rate : device->getAvailableSampleRates())
        rates.push_back (rate);

    return rates;
}

std::vector<int> Session::availableBufferSizes() const
{
    std::vector<int> sizes;
    auto* device = impl->engine.getDeviceManager().deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
        return sizes;

    for (const auto size : device->getAvailableBufferSizes())
        sizes.push_back (size);

    return sizes;
}

AudioDeviceState Session::audioDevice() const
{
    AudioDeviceState state;
    auto* device = impl->engine.getDeviceManager().deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
        return state;

    state.sampleRate = device->getCurrentSampleRate();
    state.bufferSize = device->getCurrentBufferSizeSamples();
    state.outputLatencyMs = latencyMs (device->getOutputLatencyInSamples(), state.sampleRate);
    state.inputLatencyMs = latencyMs (device->getInputLatencyInSamples(), state.sampleRate);

    // The setup names the two halves; the open device names only itself, and on
    // a driver where one device is both halves the two names are the same one.
    const auto setup = impl->engine.getDeviceManager().deviceManager.getAudioDeviceSetup();
    state.outputDevice = setup.outputDeviceName.toStdString();
    state.inputDevice = setup.inputDeviceName.toStdString();

    return state;
}

std::string Session::setAudioDevice (const AudioDeviceChoice& choice)
{
    auto& devices = impl->engine.getDeviceManager().deviceManager;
    auto setup = devices.getAudioDeviceSetup();

    // An empty name or a zero is the producer leaving that part of the device
    // where it was: choosing a buffer size is not choosing a device again.
    if (! choice.outputDevice.empty())
        setup.outputDeviceName = toJuceString (choice.outputDevice);

    if (! choice.inputDevice.empty())
        setup.inputDeviceName = toJuceString (choice.inputDevice);

    if (choice.sampleRate > 0.0)
        setup.sampleRate = choice.sampleRate;

    if (choice.bufferSize > 0)
        setup.bufferSize = choice.bufferSize;

    setup.useDefaultOutputChannels = true;
    setup.useDefaultInputChannels = true;

    const auto failure = devices.setAudioDeviceSetup (setup, true);

    if (failure.isNotEmpty())
        return failure.toStdString();

    // The engine keeps its own device lists over the ones the driver has just
    // rebuilt, and a track's input is one of them: without this the producer's
    // new device is open and nothing in Duet is routed through it.
    impl->engine.getDeviceManager().rescanWaveDeviceList();

    return {};
}

//==============================================================================
std::vector<MidiInputInfo> Session::midiInputs() const
{
    std::vector<MidiInputInfo> inputs;

    for (const auto& device : impl->engine.getDeviceManager().getMidiInDevices())
        if (device != nullptr)
            inputs.push_back ({ impl->refForInput (device->getDeviceID()),
                                device->getName().toStdString(),
                                device->isEnabled() });

    return inputs;
}

void Session::setMidiInputEnabled (InputRef input, bool enabled)
{
    auto* device = impl->inputDeviceFor (input);

    if (device != nullptr && device->isMidi())
        device->setEnabled (enabled);
}
} // namespace duet::model
