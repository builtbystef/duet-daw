#pragma once

#include <duet/model/Session.h>

#include <string>
#include <string_view>
#include <vector>

namespace duet::gui
{
class Settings;

/** The machine's audio and MIDI hardware, as the Settings window's two tabs see
    it.

    An interface for the same reason `Settings` is one: what is behind it is the
    producer's machine, and a machine with no audio hardware — a build server —
    still has to be able to say what the tabs do with one. `SessionAudioDevices`
    is the real answer, over the model; a suite supplies its own.
*/
class AudioDevices
{
public:
    virtual ~AudioDevices() = default;

    AudioDevices (const AudioDevices& other) = delete;
    AudioDevices& operator= (const AudioDevices& other) = delete;

    [[nodiscard]] virtual std::vector<std::string> outputDevices() const = 0;
    [[nodiscard]] virtual std::vector<std::string> inputDevices() const = 0;
    [[nodiscard]] virtual std::vector<double> sampleRates() const = 0;
    [[nodiscard]] virtual std::vector<int> bufferSizes() const = 0;

    /** What the device is doing right now, latency included. */
    [[nodiscard]] virtual duet::model::AudioDeviceState state() const = 0;

    /** Opens what the producer chose, and answers empty. What comes back
        otherwise is what went wrong, and then the device that was running is
        still running.
    */
    virtual std::string open (const duet::model::AudioDeviceChoice& choice) = 0;

    [[nodiscard]] virtual std::vector<duet::model::MidiInputInfo> midiInputs() const = 0;
    virtual void setMidiInputEnabled (duet::model::InputRef input, bool enabled) = 0;

protected:
    AudioDevices() = default;
};

/** The real machine, behind the model. */
class SessionAudioDevices final : public AudioDevices
{
public:
    explicit SessionAudioDevices (duet::model::Session& openProject) : session (openProject) {}

    [[nodiscard]] std::vector<std::string> outputDevices() const override;
    [[nodiscard]] std::vector<std::string> inputDevices() const override;
    [[nodiscard]] std::vector<double> sampleRates() const override;
    [[nodiscard]] std::vector<int> bufferSizes() const override;
    [[nodiscard]] duet::model::AudioDeviceState state() const override;
    std::string open (const duet::model::AudioDeviceChoice& choice) override;
    [[nodiscard]] std::vector<duet::model::MidiInputInfo> midiInputs() const override;
    void setMidiInputEnabled (duet::model::InputRef input, bool enabled) override;

private:
    duet::model::Session& session;
};

/** The Audio and MIDI tabs of the Settings window, without the painting.

    The Audio tab is one device at a time: the producer picks an output, an
    input, a rate and a buffer, each change is in force where it stands, and the
    latency the machine answers with is what the tab reports. A choice the
    machine cannot open changes nothing — the device that was running is still
    running — and what went wrong is `lastProblem`, which is what the tab shows
    them.

    Every choice is app-global: it is the producer's machine and not their
    project, so it is stored here and opened again on the next launch through
    `applyStoredChoice`.

    The MIDI tab is the machine's MIDI inputs and a switch on each. An input that
    is switched off reaches no track, and the engine remembers the switch itself
    — the devices are its own, and their state outlives every project that runs
    over them.
*/
class AudioMidiSettings
{
public:
    /** @param store  the app-global store the producer's device choice lives in
    */
    explicit AudioMidiSettings (Settings& store);

    ~AudioMidiSettings() = default;

    AudioMidiSettings (const AudioMidiSettings& other) = delete;
    AudioMidiSettings& operator= (const AudioMidiSettings& other) = delete;

    /** The machine the tabs set, and nothing when there is none — which is what
        the tabs look like before a project has opened.
    */
    void setDevices (AudioDevices* machine);

    /** Opens the device the producer chose on an earlier launch, if they chose
        one and the machine still has it. Empty when there was nothing to do or
        it worked, and what went wrong otherwise.

        Called once as the machine arrives: this is the whole of what "persists
        across restarts" means.
    */
    std::string applyStoredChoice();

    //==============================================================================
    [[nodiscard]] std::vector<std::string> outputDevices() const;
    [[nodiscard]] std::vector<std::string> inputDevices() const;
    [[nodiscard]] std::vector<double> sampleRates() const;
    [[nodiscard]] std::vector<int> bufferSizes() const;

    [[nodiscard]] std::string outputDevice() const;
    [[nodiscard]] std::string inputDevice() const;
    [[nodiscard]] double sampleRate() const;
    [[nodiscard]] int bufferSize() const;

    /** What the machine answers about how long a sample takes to get out of it
        and in, in milliseconds.
    */
    [[nodiscard]] double outputLatencyMs() const;
    [[nodiscard]] double inputLatencyMs() const;

    /** The same, as the row reads: "11.6 ms out, 5.8 ms in", and empty when no
        device is open.
    */
    [[nodiscard]] std::string latencyText() const;

    //==============================================================================
    /** Each of these opens the device with that one part of it changed, stores
        the choice for the next launch, and answers whether it worked. A false
        leaves the previous device running and `lastProblem` saying why.
    */
    bool setOutputDevice (std::string_view device);
    bool setInputDevice (std::string_view device);
    bool setSampleRate (double rate);
    bool setBufferSize (int samples);

    /** What went wrong the last time the producer chose something, and empty
        when the last choice worked.
    */
    [[nodiscard]] const std::string& lastProblem() const { return problem; }

    //==============================================================================
    /** The machine's MIDI inputs, switched on or off. */
    [[nodiscard]] std::vector<duet::model::MidiInputInfo> midiInputs() const;

    void setMidiInputEnabled (duet::model::InputRef input, bool enabled);
    [[nodiscard]] bool isMidiInputEnabled (duet::model::InputRef input) const;

private:
    /** Opens a choice, remembers it when it opened, and records the problem when
        it did not.
    */
    bool apply (const duet::model::AudioDeviceChoice& choice);

    Settings& settings;
    AudioDevices* devices = nullptr;
    std::string problem;
};
} // namespace duet::gui
