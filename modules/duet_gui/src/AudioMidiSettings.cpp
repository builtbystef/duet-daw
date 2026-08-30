#include <duet/gui/AudioMidiSettings.h>

#include <duet/gui/Settings.h>

#include <algorithm>
#include <exception>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace duet::gui
{
namespace
{
    constexpr std::string_view outputDeviceKey = "audio.outputDevice";
    constexpr std::string_view inputDeviceKey = "audio.inputDevice";
    constexpr std::string_view sampleRateKey = "audio.sampleRate";
    constexpr std::string_view bufferSizeKey = "audio.bufferSize";

    /** How many decimals a latency is reported to. Tenths of a millisecond:
        finer than that is below what a buffer size can change it by.
    */
    constexpr int latencyDecimals = 1;

    std::optional<double> storedNumber (const Settings& store, std::string_view key)
    {
        const auto held = store.value (key);

        if (! held.has_value() || held->empty())
            return {};

        try
        {
            return std::stod (*held);
        }
        catch (const std::exception&)
        {
            return {};
        }
    }

    std::string numberText (double value)
    {
        std::ostringstream text;
        text << std::fixed << std::setprecision (latencyDecimals) << value;

        return std::move (text).str();
    }
} // namespace

//==============================================================================
std::vector<std::string> SessionAudioDevices::outputDevices() const
{
    return session.availableOutputDevices();
}

std::vector<std::string> SessionAudioDevices::inputDevices() const
{
    return session.availableInputDevices();
}

std::vector<double> SessionAudioDevices::sampleRates() const
{
    return session.availableSampleRates();
}

std::vector<int> SessionAudioDevices::bufferSizes() const { return session.availableBufferSizes(); }

duet::model::AudioDeviceState SessionAudioDevices::state() const { return session.audioDevice(); }

std::string SessionAudioDevices::open (const duet::model::AudioDeviceChoice& choice)
{
    return session.setAudioDevice (choice);
}

std::vector<duet::model::MidiInputInfo> SessionAudioDevices::midiInputs() const
{
    return session.midiInputs();
}

void SessionAudioDevices::setMidiInputEnabled (duet::model::InputRef input, bool enabled)
{
    session.setMidiInputEnabled (input, enabled);
}

//==============================================================================
AudioMidiSettings::AudioMidiSettings (Settings& store) : settings (store) {}

void AudioMidiSettings::setDevices (AudioDevices* machine)
{
    devices = machine;
    problem.clear();
}

std::string AudioMidiSettings::applyStoredChoice()
{
    if (devices == nullptr)
        return {};

    duet::model::AudioDeviceChoice stored;

    if (const auto output = settings.value (outputDeviceKey); output.has_value())
        stored.outputDevice = *output;

    if (const auto input = settings.value (inputDeviceKey); input.has_value())
        stored.inputDevice = *input;

    if (const auto rate = storedNumber (settings, sampleRateKey); rate.has_value())
        stored.sampleRate = *rate;

    if (const auto buffer = storedNumber (settings, bufferSizeKey); buffer.has_value())
        stored.bufferSize = static_cast<int> (*buffer);

    // Nothing stored is a first launch, and a first launch takes the machine as
    // the driver hands it over.
    if (stored.outputDevice.empty() && stored.inputDevice.empty() && stored.sampleRate <= 0.0
        && stored.bufferSize <= 0)
        return {};

    problem = devices->open (stored);

    return problem;
}

//==============================================================================
std::vector<std::string> AudioMidiSettings::outputDevices() const
{
    return devices != nullptr ? devices->outputDevices() : std::vector<std::string> {};
}

std::vector<std::string> AudioMidiSettings::inputDevices() const
{
    return devices != nullptr ? devices->inputDevices() : std::vector<std::string> {};
}

std::vector<double> AudioMidiSettings::sampleRates() const
{
    return devices != nullptr ? devices->sampleRates() : std::vector<double> {};
}

std::vector<int> AudioMidiSettings::bufferSizes() const
{
    return devices != nullptr ? devices->bufferSizes() : std::vector<int> {};
}

std::string AudioMidiSettings::outputDevice() const
{
    return devices != nullptr ? devices->state().outputDevice : std::string {};
}

std::string AudioMidiSettings::inputDevice() const
{
    return devices != nullptr ? devices->state().inputDevice : std::string {};
}

double AudioMidiSettings::sampleRate() const
{
    return devices != nullptr ? devices->state().sampleRate : 0.0;
}

int AudioMidiSettings::bufferSize() const
{
    return devices != nullptr ? devices->state().bufferSize : 0;
}

double AudioMidiSettings::outputLatencyMs() const
{
    return devices != nullptr ? devices->state().outputLatencyMs : 0.0;
}

double AudioMidiSettings::inputLatencyMs() const
{
    return devices != nullptr ? devices->state().inputLatencyMs : 0.0;
}

std::string AudioMidiSettings::latencyText() const
{
    if (devices == nullptr)
        return {};

    const auto machine = devices->state();

    if (machine.sampleRate <= 0.0)
        return {};

    return numberText (machine.outputLatencyMs) + " ms out, " + numberText (machine.inputLatencyMs)
           + " ms in";
}

//==============================================================================
bool AudioMidiSettings::setOutputDevice (std::string_view device)
{
    duet::model::AudioDeviceChoice choice;
    choice.outputDevice = std::string { device };

    return apply (choice);
}

bool AudioMidiSettings::setInputDevice (std::string_view device)
{
    duet::model::AudioDeviceChoice choice;
    choice.inputDevice = std::string { device };

    return apply (choice);
}

bool AudioMidiSettings::setSampleRate (double rate)
{
    duet::model::AudioDeviceChoice choice;
    choice.sampleRate = rate;

    return apply (choice);
}

bool AudioMidiSettings::setBufferSize (int samples)
{
    duet::model::AudioDeviceChoice choice;
    choice.bufferSize = samples;

    return apply (choice);
}

bool AudioMidiSettings::apply (const duet::model::AudioDeviceChoice& choice)
{
    if (devices == nullptr)
        return false;

    problem = devices->open (choice);

    if (! problem.empty())
        return false;

    // What the machine actually opened, and not what was asked for: a driver
    // that answers a rate with the nearest one it has is what the next launch
    // should ask for.
    const auto machine = devices->state();

    settings.setValue (outputDeviceKey, machine.outputDevice);
    settings.setValue (inputDeviceKey, machine.inputDevice);
    settings.setValue (sampleRateKey, numberText (machine.sampleRate));
    settings.setValue (bufferSizeKey, std::to_string (machine.bufferSize));

    return true;
}

//==============================================================================
std::vector<duet::model::MidiInputInfo> AudioMidiSettings::midiInputs() const
{
    return devices != nullptr ? devices->midiInputs() : std::vector<duet::model::MidiInputInfo> {};
}

void AudioMidiSettings::setMidiInputEnabled (duet::model::InputRef input, bool enabled)
{
    if (devices != nullptr)
        devices->setMidiInputEnabled (input, enabled);
}

bool AudioMidiSettings::isMidiInputEnabled (duet::model::InputRef input) const
{
    const auto inputs = midiInputs();
    const auto found = std::ranges::find_if (
        inputs, [input] (const duet::model::MidiInputInfo& one) { return one.input == input; });

    return found != inputs.end() && found->enabled;
}
} // namespace duet::gui
