#include <duet/gui/TrackIo.h>

#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

using duet::gui::InputChoice;
using duet::gui::TrackIo;
using duet::model::InputKind;
using duet::model::InputMonitoring;
using duet::model::InputRef;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;

namespace
{
InputRef inputOfKind (const Session& session, InputKind kind)
{
    for (const auto& input : session.availableInputs())
        if (input.kind == kind)
            return input.input;

    return duet::model::noInput;
}

const InputChoice* selectedInput (const std::vector<InputChoice>& choices)
{
    const auto found = std::find_if (
        choices.begin(), choices.end(), [] (const auto& choice) { return choice.selected; });
    return found == choices.end() ? nullptr : &*found;
}

bool hasKind (const Session& session, InputRef input, InputKind kind)
{
    for (const auto& candidate : session.availableInputs())
        if (candidate.input == input)
            return candidate.kind == kind;

    return false;
}
} // namespace

TEST_CASE ("Track I/O lists None then compatible devices for audio, MIDI, and group tracks")
{
    const TempProject project;
    Session session { project.editFile() };
    session.useNoAudioDevice();

    TrackRef audio = duet::model::noTrack;
    TrackRef midi = duet::model::noTrack;
    TrackRef group = duet::model::noTrack;
    session.performAction ("Tracks",
                           [&] (auto& ops)
                           {
                               audio = ops.createTrack (TrackKind::audio, "Guitar");
                               midi = ops.createTrack (TrackKind::midi, "Keys");
                               group = ops.createTrack (TrackKind::group, "Bus");
                           });

    TrackIo io;
    io.setSession (&session);

    const auto audioSnap = io.snapshot (audio);
    REQUIRE (audioSnap.kind == TrackKind::audio);
    REQUIRE_FALSE (audioSnap.inputs.empty());
    REQUIRE (audioSnap.inputs.front().label == "None");
    REQUIRE (audioSnap.inputs.front().selected);
    REQUIRE (audioSnap.inputs.front().enabled);
    for (std::size_t index = 1; index < audioSnap.inputs.size(); ++index)
    {
        REQUIRE (audioSnap.inputs[index].enabled);
        REQUIRE (hasKind (session, audioSnap.inputs[index].input, InputKind::audio));
    }
    REQUIRE_FALSE (audioSnap.armAvailable);
    REQUIRE_FALSE (audioSnap.monitoring.enabled);
    REQUIRE (audioSnap.monitoring.mode == InputMonitoring::whileArmed);

    const auto midiSnap = io.snapshot (midi);
    REQUIRE (midiSnap.inputs.front().label == "None");
    REQUIRE (midiSnap.inputs.front().selected);
    for (std::size_t index = 1; index < midiSnap.inputs.size(); ++index)
        REQUIRE (hasKind (session, midiSnap.inputs[index].input, InputKind::midi));

    const auto groupSnap = io.snapshot (group);
    REQUIRE (groupSnap.inputs.size() == 1);
    REQUIRE (groupSnap.inputs.front().label == "None");
    REQUIRE (groupSnap.inputs.front().selected);
    REQUIRE_FALSE (groupSnap.inputs.front().enabled);
    REQUIRE_FALSE (groupSnap.armAvailable);
    REQUIRE_FALSE (groupSnap.monitoring.enabled);
}

TEST_CASE ("a missing device stays selected as Unavailable and does not pick another input")
{
    const TempProject project;
    Session session { project.editFile() };
    session.useNoAudioDevice();

    duet::model::InputInfo midi;
    for (const auto& input : session.availableInputs())
        if (input.kind == InputKind::midi)
            midi = input;
    REQUIRE (midi.input != duet::model::noInput);

    TrackRef keys = duet::model::noTrack;
    session.performAction ("Add a track",
                           [&] (auto& ops) { keys = ops.createTrack (TrackKind::midi, "Keys"); });

    TrackIo io;
    io.setSession (&session);
    io.setInput (keys, midi.input);
    io.setArmed (keys, true);

    session.setMidiInputEnabled (midi.input, false);

    const auto snap = io.snapshot (keys);
    const auto* selected = selectedInput (snap.inputs);
    REQUIRE (selected != nullptr);
    REQUIRE (selected->input == midi.input);
    REQUIRE (selected->label == "Unavailable — " + midi.name);
    REQUIRE_FALSE (selected->enabled);
    REQUIRE_FALSE (snap.armAvailable);
    REQUIRE_FALSE (snap.armed);
    REQUIRE_FALSE (snap.monitoring.enabled);
    REQUIRE (snap.monitoring.mode == InputMonitoring::whileArmed);
    REQUIRE (session.track (keys).input == midi.input);

    session.setMidiInputEnabled (midi.input, true);
    const auto restored = io.snapshot (keys);
    const auto* live = selectedInput (restored.inputs);
    REQUIRE (live != nullptr);
    REQUIRE (live->input == midi.input);
    REQUIRE (live->label == midi.name);
    REQUIRE (live->enabled);
    REQUIRE (restored.armAvailable);
    REQUIRE_FALSE (restored.armed);
}

TEST_CASE ("monitoring follows the selected input and ignores writes when none is available")
{
    const TempProject project;
    Session session { project.editFile() };
    session.useNoAudioDevice();

    const auto midiInput = inputOfKind (session, InputKind::midi);
    REQUIRE (midiInput != duet::model::noInput);

    TrackRef keys = duet::model::noTrack;
    session.performAction ("Add a track",
                           [&] (auto& ops) { keys = ops.createTrack (TrackKind::midi, "Keys"); });

    TrackIo io;
    io.setSession (&session);
    io.setMonitoring (keys, InputMonitoring::on);
    REQUIRE (io.snapshot (keys).monitoring.mode == InputMonitoring::whileArmed);
    REQUIRE_FALSE (io.snapshot (keys).monitoring.enabled);

    io.setInput (keys, midiInput);
    REQUIRE (io.snapshot (keys).monitoring.enabled);
    io.setMonitoring (keys, InputMonitoring::on);
    REQUIRE (io.snapshot (keys).monitoring.mode == InputMonitoring::on);
    REQUIRE (session.inputMonitoring (midiInput) == InputMonitoring::on);

    io.setInput (keys, duet::model::noInput);
    REQUIRE (io.snapshot (keys).monitoring.mode == InputMonitoring::whileArmed);
    REQUIRE_FALSE (io.snapshot (keys).monitoring.enabled);
    io.setMonitoring (keys, InputMonitoring::off);
    REQUIRE (io.snapshot (keys).monitoring.mode == InputMonitoring::whileArmed);
    REQUIRE (session.inputMonitoring (midiInput) == InputMonitoring::on);
}

TEST_CASE ("Track I/O writes are no-ops when repeated or invalid")
{
    const TempProject project;
    Session session { project.editFile() };
    session.useNoAudioDevice();

    const auto midiInput = inputOfKind (session, InputKind::midi);
    const auto audioInput = inputOfKind (session, InputKind::audio);
    REQUIRE (midiInput != duet::model::noInput);
    REQUIRE (audioInput != duet::model::noInput);

    TrackRef keys = duet::model::noTrack;
    TrackRef group = duet::model::noTrack;
    session.performAction ("Tracks",
                           [&] (auto& ops)
                           {
                               keys = ops.createTrack (TrackKind::midi, "Keys");
                               group = ops.createTrack (TrackKind::group, "Bus");
                           });

    TrackIo io;
    io.setSession (&session);
    io.setInput (keys, midiInput);
    io.setArmed (keys, true);

    std::vector<std::string> notices;
    session.onEngineMessage ([&] (const std::string& text) { notices.push_back (text); });

    io.setInput (keys, midiInput);
    io.setArmed (keys, true);
    REQUIRE (notices.empty());
    REQUIRE (session.track (keys).input == midiInput);
    REQUIRE (session.track (keys).recordArmed);

    io.setInput (keys, audioInput);
    REQUIRE (session.track (keys).input == midiInput);
    REQUIRE_FALSE (notices.empty());

    notices.clear();
    io.setArmed (group, true);
    REQUIRE_FALSE (session.track (group).recordArmed);
    REQUIRE_FALSE (notices.empty());
}

TEST_CASE ("an output write is one Set Track Output Action with digest-exact undo")
{
    const TempProject project;
    Session session { project.editFile() };
    TrackRef audio = duet::model::noTrack;
    TrackRef group = duet::model::noTrack;
    session.performAction ("Tracks",
                           [&] (auto& ops)
                           {
                               audio = ops.createTrack (TrackKind::audio, "Guitar");
                               group = ops.createTrack (TrackKind::group, "Bus");
                           });

    TrackIo io;
    io.setSession (&session);

    const auto before = session.stateDigest();
    const auto undoBefore = session.undoNames();
    io.setOutput (audio, group);
    REQUIRE (session.track (audio).output == group);
    REQUIRE (session.undoNames().front() == "Set Track Output");
    REQUIRE (io.snapshot (audio).output == group);

    io.setOutput (audio, group);
    REQUIRE (session.undoNames().size() == undoBefore.size() + 1);

    REQUIRE (session.undo());
    REQUIRE (session.stateDigest() == before);
    REQUIRE (session.track (audio).output == duet::model::noTrack);

    io.setOutput (audio, audio);
    REQUIRE (session.track (audio).output == duet::model::noTrack);
    REQUIRE (session.undoNames() == undoBefore);
}
