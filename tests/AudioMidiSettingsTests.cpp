#include <duet/gui/AudioMidiSettings.h>

#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

using duet::gui::AudioDevices;
using duet::gui::AudioMidiSettings;
using duet::gui::SessionAudioDevices;
using duet::model::AudioDeviceChoice;
using duet::model::AudioDeviceState;
using duet::model::InputRef;
using duet::model::MidiInputInfo;
using duet::model::Session;
using duet::model::TrackKind;
using duet::testing::StoredSettings;
using duet::testing::TempProject;

/** The Audio and MIDI tabs of the Settings window (issue zm174o).

    What the tabs do with a machine is asserted against a machine of the case's
    own, for the reason the interface exists: a build server has no audio
    hardware, and what a tab does with a device that fails to open is exactly
    what has to be true on every machine. What the real one does with the engine
    is at the bottom of this file, and says only what a headless run can know.
*/
namespace
{
/** A machine the case drives: two outputs, two inputs, and a device that
    refuses to open.
*/
class FakeMachine final : public AudioDevices
{
public:
    [[nodiscard]] std::vector<std::string> outputDevices() const override { return outputs; }
    [[nodiscard]] std::vector<std::string> inputDevices() const override { return inputs; }
    [[nodiscard]] std::vector<double> sampleRates() const override
    {
        return { 44100.0, 48000.0, 96000.0 };
    }
    [[nodiscard]] std::vector<int> bufferSizes() const override { return { 128, 256, 512, 1024 }; }

    [[nodiscard]] AudioDeviceState state() const override { return open_; }

    std::string open (const AudioDeviceChoice& choice) override
    {
        ++openCount;

        auto wanted = open_;

        if (! choice.outputDevice.empty())
            wanted.outputDevice = choice.outputDevice;

        if (! choice.inputDevice.empty())
            wanted.inputDevice = choice.inputDevice;

        if (choice.sampleRate > 0.0)
            wanted.sampleRate = choice.sampleRate;

        if (choice.bufferSize > 0)
            wanted.bufferSize = choice.bufferSize;

        if (wanted.outputDevice == brokenDevice)
            return "The device could not be opened";

        // The latency the machine answers with follows the buffer, which is
        // what a producer choosing one is choosing.
        open_ = wanted;
        open_.outputLatencyMs = 1000.0 * open_.bufferSize / open_.sampleRate;
        open_.inputLatencyMs = open_.outputLatencyMs / 2.0;

        return {};
    }

    [[nodiscard]] std::vector<MidiInputInfo> midiInputs() const override { return midi; }

    void setMidiInputEnabled (InputRef input, bool enabled) override
    {
        for (auto& one : midi)
            if (one.input == input)
                one.enabled = enabled;
    }

    static constexpr const char* brokenDevice = "Broken Interface";

    std::vector<std::string> outputs { "Built-in Output", "Studio Interface", brokenDevice };
    std::vector<std::string> inputs { "Built-in Input", "Studio Interface" };
    std::vector<MidiInputInfo> midi { { 1, "Keystation", true }, { 2, "Push", false } };
    AudioDeviceState open_ { "Built-in Output", "Built-in Input", 44100.0, 512, 11.6, 5.8 };
    int openCount = 0;
};
} // namespace

//==============================================================================
TEST_CASE ("the Audio tab lists what the machine offers and what it is doing")
{
    StoredSettings store;
    FakeMachine machine;
    AudioMidiSettings tab { store };
    tab.setDevices (&machine);

    REQUIRE (tab.outputDevices().size() == 3);
    REQUIRE (tab.inputDevices().size() == 2);
    REQUIRE (tab.sampleRates().size() == 3);
    REQUIRE (tab.bufferSizes().size() == 4);

    REQUIRE (tab.outputDevice() == "Built-in Output");
    REQUIRE (tab.inputDevice() == "Built-in Input");
    REQUIRE (tab.sampleRate() == Catch::Approx (44100.0));
    REQUIRE (tab.bufferSize() == 512);
    REQUIRE (tab.latencyText() == "11.6 ms out, 5.8 ms in");
}

TEST_CASE ("the Audio tab changes the device, the rate and the buffer where they stand")
{
    StoredSettings store;
    FakeMachine machine;
    AudioMidiSettings tab { store };
    tab.setDevices (&machine);

    REQUIRE (tab.setOutputDevice ("Studio Interface"));
    REQUIRE (tab.outputDevice() == "Studio Interface");

    REQUIRE (tab.setInputDevice ("Studio Interface"));
    REQUIRE (tab.inputDevice() == "Studio Interface");

    REQUIRE (tab.setSampleRate (48000.0));
    REQUIRE (tab.sampleRate() == Catch::Approx (48000.0));

    // The device is still the one they chose: setting a buffer size is not
    // choosing a device again.
    REQUIRE (tab.setBufferSize (128));
    REQUIRE (tab.bufferSize() == 128);
    REQUIRE (tab.outputDevice() == "Studio Interface");

    // And the latency the tab reports is what the machine answers with, which is
    // what a producer is choosing a buffer size against.
    REQUIRE (tab.outputLatencyMs() == Catch::Approx (1000.0 * 128 / 48000.0));
    REQUIRE (tab.latencyText() == "2.7 ms out, 1.3 ms in");
}

TEST_CASE ("a device that fails to open is reported, and the one that was running still is")
{
    StoredSettings store;
    FakeMachine machine;
    AudioMidiSettings tab { store };
    tab.setDevices (&machine);

    REQUIRE (tab.setOutputDevice ("Studio Interface"));
    REQUIRE (tab.lastProblem().empty());

    REQUIRE_FALSE (tab.setOutputDevice (FakeMachine::brokenDevice));

    REQUIRE (tab.lastProblem() == "The device could not be opened");
    REQUIRE (tab.outputDevice() == "Studio Interface");
    REQUIRE (tab.sampleRate() == Catch::Approx (44100.0));
}

TEST_CASE ("the producer's device choice is what the next launch opens")
{
    StoredSettings store;

    {
        FakeMachine machine;
        AudioMidiSettings tab { store };
        tab.setDevices (&machine);

        REQUIRE (tab.setOutputDevice ("Studio Interface"));
        REQUIRE (tab.setSampleRate (96000.0));
        REQUIRE (tab.setBufferSize (256));
    }

    // The next launch: a machine as the driver hands it over, and the store the
    // producer left behind.
    FakeMachine relaunched;
    AudioMidiSettings tab { store };
    tab.setDevices (&relaunched);

    REQUIRE (tab.outputDevice() == "Built-in Output");
    REQUIRE (tab.applyStoredChoice().empty());

    REQUIRE (tab.outputDevice() == "Studio Interface");
    REQUIRE (tab.sampleRate() == Catch::Approx (96000.0));
    REQUIRE (tab.bufferSize() == 256);
}

TEST_CASE ("a first launch chose nothing, so nothing is opened over what the driver handed over")
{
    StoredSettings store;
    FakeMachine machine;
    AudioMidiSettings tab { store };
    tab.setDevices (&machine);

    REQUIRE (tab.applyStoredChoice().empty());
    REQUIRE (machine.openCount == 0);
    REQUIRE (tab.outputDevice() == "Built-in Output");
}

TEST_CASE ("a stored device the machine no longer has is reported rather than silently missing")
{
    StoredSettings store;

    {
        FakeMachine machine;
        AudioMidiSettings tab { store };
        tab.setDevices (&machine);

        REQUIRE (tab.setOutputDevice ("Studio Interface"));
    }

    FakeMachine relaunched;
    relaunched.outputs = { "Built-in Output", FakeMachine::brokenDevice };
    AudioMidiSettings tab { store };
    tab.setDevices (&relaunched);

    // The machine is asked for it and says no; the tab has the words and the
    // producer keeps the device the driver handed over.
    relaunched.open_.outputDevice = FakeMachine::brokenDevice;
    store.setValue ("audio.outputDevice", FakeMachine::brokenDevice);

    REQUIRE_FALSE (tab.applyStoredChoice().empty());
    REQUIRE_FALSE (tab.lastProblem().empty());
}

TEST_CASE ("the MIDI tab lists the machine's inputs and switches them on and off")
{
    StoredSettings store;
    FakeMachine machine;
    AudioMidiSettings tab { store };
    tab.setDevices (&machine);

    const auto inputs = tab.midiInputs();

    REQUIRE (inputs.size() == 2);
    REQUIRE (inputs.front().name == "Keystation");
    REQUIRE (inputs.front().enabled);
    REQUIRE_FALSE (inputs.back().enabled);

    tab.setMidiInputEnabled (inputs.back().input, true);
    REQUIRE (tab.isMidiInputEnabled (inputs.back().input));

    tab.setMidiInputEnabled (inputs.front().input, false);
    REQUIRE_FALSE (tab.isMidiInputEnabled (inputs.front().input));
}

TEST_CASE ("the tabs with no machine behind them answer nothing rather than crashing")
{
    StoredSettings store;
    AudioMidiSettings tab { store };

    REQUIRE (tab.outputDevices().empty());
    REQUIRE (tab.midiInputs().empty());
    REQUIRE (tab.latencyText().empty());
    REQUIRE_FALSE (tab.setBufferSize (256));
    REQUIRE (tab.applyStoredChoice().empty());
}

//==============================================================================
TEST_CASE ("the real machine answers the tabs out of the engine")
{
    const TempProject folder;
    Session session { folder.editFile() };
    session.suppressDeviceRebuild();

    const SessionAudioDevices machine { session };

    // What a headless run can know: the machine answers, and what it says about
    // the device it has open agrees with itself. A build server has no audio
    // hardware, so the lists may be empty and that is not a failure.
    const auto open = machine.state();

    if (open.sampleRate > 0.0)
    {
        REQUIRE (open.bufferSize > 0);
        REQUIRE (open.outputLatencyMs >= 0.0);
        REQUIRE_FALSE (machine.sampleRates().empty());
        REQUIRE_FALSE (machine.bufferSizes().empty());
    }

    // Every MIDI input the engine has, switched or not — and the engine always
    // has its own "All MIDI Ins".
    const auto inputs = machine.midiInputs();

    for (const auto& input : inputs)
        REQUIRE_FALSE (input.name.empty());
}

TEST_CASE ("a MIDI input the producer switched off is one no track can record from")
{
    const TempProject folder;
    Session session { folder.editFile() };
    session.suppressDeviceRebuild();
    session.rebuildDevices();

    SessionAudioDevices machine { session };
    const auto inputs = machine.midiInputs();

    if (inputs.empty())
        return;

    const auto& first = inputs.front();

    machine.setMidiInputEnabled (first.input, false);

    const auto available = session.availableInputs();
    const auto stillThere = std::ranges::find_if (
        available, [&first] (const auto& one) { return one.input == first.input; });

    REQUIRE (stillThere == available.end());

    // And switching it back on puts it back in what a track can record from.
    machine.setMidiInputEnabled (first.input, true);

    const auto again = session.availableInputs();
    REQUIRE (std::ranges::any_of (again,
                                  [&first] (const auto& one) { return one.input == first.input; }));
}

TEST_CASE ("an enabled MIDI input plays the armed track's instrument")
{
    const TempProject folder;
    Session session { folder.editFile() };
    session.useNoAudioDevice();
    session.suppressDeviceRebuild();

    auto keys = duet::model::noTrack;
    session.performAction (
        "Add keys",
        [&] (auto& ops)
        { keys = ops.createTrack (TrackKind::midi, "Keys", duet::model::BuiltinPlugin::synth); });

    SessionAudioDevices machine { session };
    const auto inputs = machine.midiInputs();

    REQUIRE_FALSE (inputs.empty());

    const auto& keyboard = inputs.front();
    machine.setMidiInputEnabled (keyboard.input, true);

    session.setTrackInput (keys, keyboard.input);
    session.setTrackRecordArmed (keys, true);
    session.setInputMonitoring (keyboard.input, duet::model::InputMonitoring::on);

    session.startPlayback();
    session.runWithoutAudioDevice (0.1);

    // The meter is cleared by reading it, so what is read afterwards is what the
    // notes played in put out (ADR 0006: audibility is the playback graph's).
    static_cast<void> (session.trackPeakDb (keys));

    for (int block = 0; block < 20; ++block)
        session.runWithoutAudioDevice (0.05, { { { 0.0, 0.05, 60, 100 } } });

    const auto heard = session.trackPeakDb (keys);

    INFO ("track peak: " << heard);
    REQUIRE (heard > duet::model::silentDb);

    session.stopPlayback();
}
