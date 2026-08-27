#include <duet/model/Session.h>

#include <duet/persistence/Project.h>
#include <duet/persistence/ProjectLayout.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;
using duet::model::InputKind;
using duet::model::InputMonitoring;
using duet::model::InputRef;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;

namespace
{
/** How many half-seconds of a take it may take before the transport reports a
    playhead that has moved.
*/
constexpr int playheadAttempts = 10;

/** How long one of those attempts gives the message loop to report the move.

    Blocks are what move the playhead, so an attempt that runs out costs the
    answer nothing: the next one pushes more blocks and asks again. What this
    bounds is only how long a take is left rolling through a message loop with
    no blocks going through it, and that is exposure to be spent sparingly —
    every device apply that lands there ends the take.
*/
constexpr int msPerPlayheadAttempt = 100;

/** Where the audio take starts: anywhere but the start of the timeline. */
constexpr double takeStartSeconds = 4.0;

/** How long a real device may take to offer its inputs, which it does from the
    message loop.
*/
constexpr int inputAttempts = 20;
constexpr int msPerInputAttempt = 50;

/** The input of a kind that a session running without audio hardware offers. */
InputRef inputOfKind (const Session& session, InputKind kind)
{
    for (const auto& input : session.availableInputs())
        if (input.kind == kind)
            return input.input;

    return duet::model::noInput;
}

/** Runs a take until the transport says where its playhead has got to.

    With no audio device the blocks and the message loop do not run at the same
    time, and the position a transport reports is the one that loop last fetched
    from the graph — so a take has to be run and the loop let run, until there
    is a position to read.
*/
double runUntilThePlayheadMoves (Session& session)
{
    const auto moved = [&session] { return session.playbackPositionSeconds() > 0.0; };

    for (int attempt = 0; attempt < playheadAttempts && ! moved(); ++attempt)
    {
        session.runWithoutAudioDevice (0.5);
        duet::testing::pumpUntil (moved, msPerPlayheadAttempt);
    }

    return session.playbackPositionSeconds();
}
} // namespace

TEST_CASE ("a track is armed to record from an input, and says which one")
{
    const TempProject project;
    Session session { project.editFile() };
    session.useNoAudioDevice();

    const auto midiInput = inputOfKind (session, InputKind::midi);
    const auto audioInput = inputOfKind (session, InputKind::audio);

    REQUIRE (midiInput != duet::model::noInput);
    REQUIRE (audioInput != duet::model::noInput);

    TrackRef keys = duet::model::noTrack;

    session.performAction ("Add a track",
                           [&] (auto& ops) { keys = ops.createTrack (TrackKind::midi, "Keys"); });

    // A track records from nothing until it is told where to record from.
    REQUIRE (session.track (keys).input == duet::model::noInput);
    REQUIRE_FALSE (session.track (keys).recordArmed);

    session.setTrackInput (keys, midiInput);
    REQUIRE (session.track (keys).input == midiInput);

    session.setTrackRecordArmed (keys, true);
    REQUIRE (session.track (keys).recordArmed);

    // A track records from one input, so choosing another takes the first away.
    session.setTrackInput (keys, audioInput);
    REQUIRE (session.track (keys).input == audioInput);

    session.setTrackRecordArmed (keys, false);
    REQUIRE_FALSE (session.track (keys).recordArmed);

    session.setTrackInput (keys, duet::model::noInput);
    REQUIRE (session.track (keys).input == duet::model::noInput);
}

TEST_CASE ("an input says how much of it the producer hears")
{
    const TempProject project;
    Session session { project.editFile() };
    session.useNoAudioDevice();

    const auto midiInput = inputOfKind (session, InputKind::midi);
    REQUIRE (midiInput != duet::model::noInput);

    session.setInputMonitoring (midiInput, InputMonitoring::on);
    REQUIRE (session.inputMonitoring (midiInput) == InputMonitoring::on);

    session.setInputMonitoring (midiInput, InputMonitoring::off);
    REQUIRE (session.inputMonitoring (midiInput) == InputMonitoring::off);

    session.setInputMonitoring (midiInput, InputMonitoring::whileArmed);
    REQUIRE (session.inputMonitoring (midiInput) == InputMonitoring::whileArmed);
}

TEST_CASE ("an undo during a take neither stops it nor moves the playhead")
{
    const TempProject project;
    Session session { project.editFile() };
    session.useNoAudioDevice();

    const auto midiInput = inputOfKind (session, InputKind::midi);
    REQUIRE (midiInput != duet::model::noInput);

    TrackRef keys = duet::model::noTrack;

    session.performAction (
        "Add a track",
        [&] (auto& ops)
        { keys = ops.createTrack (TrackKind::midi, "Keys", duet::model::BuiltinPlugin::synth); });
    session.performAction ("Rename the track", [&] (auto& ops) { ops.renameTrack (keys, "Pads"); });

    session.setTrackInput (keys, midiInput);
    session.setTrackRecordArmed (keys, true);

    // Record on settled devices starts the take at once; on devices that have
    // only just gone quiet it waits out the rest of the pre-roll first. Which
    // of the two this is is not what the case is about, so it waits for the
    // take rather than assuming either.
    session.startRecording();
    REQUIRE (duet::testing::pumpUntil ([&] { return session.isRecording(); }));

    const auto reached = runUntilThePlayheadMoves (session);
    REQUIRE (reached > 0.0);
    REQUIRE (session.isRecording());

    // The transport is written with no undo history at all, so there is nothing
    // in the Action this undoes for the take to be caught up in.
    REQUIRE (session.undo());

    REQUIRE (session.isRecording());
    REQUIRE_THAT (session.playbackPositionSeconds(), WithinAbs (reached, 0.001));
    REQUIRE (session.track (keys).name == "Keys");

    session.stopRecording();
    REQUIRE_FALSE (session.isRecording());
}

TEST_CASE ("a recorded MIDI take lands as one Action, and one undo takes it away")
{
    const TempProject project;
    Session session { project.editFile() };
    session.useNoAudioDevice();

    const auto midiInput = inputOfKind (session, InputKind::midi);
    REQUIRE (midiInput != duet::model::noInput);

    TrackRef keys = duet::model::noTrack;

    session.performAction (
        "Add a track",
        [&] (auto& ops)
        { keys = ops.createTrack (TrackKind::midi, "Keys", duet::model::BuiltinPlugin::synth); });

    session.setTrackInput (keys, midiInput);
    session.setTrackRecordArmed (keys, true);

    const auto beforeTheTake = session.stateDigest();

    // Four notes of a chord shape, a beat apart at the project's 120 bpm.
    const std::vector<duet::model::InputNote> played {
        { 0.5, 0.4, 60, 100 }, { 1.0, 0.4, 64, 100 }, { 1.5, 0.4, 67, 100 }, { 2.0, 0.4, 72, 100 }
    };

    session.startRecording();
    session.runWithoutAudioDevice (3.0, { played, 0.0, 0.0 });
    session.stopRecording();

    REQUIRE_FALSE (session.isRecording());

    const auto clips = session.track (keys).clips;
    REQUIRE (clips.size() == 1);
    REQUIRE (clips.front().holdsMidi);

    const auto recorded = session.notes (clips.front().clip);
    REQUIRE (recorded.size() == played.size());

    for (std::size_t note = 0; note < played.size(); ++note)
        REQUIRE (recorded[note].pitch == played[note].pitch);

    // The whole take is one step, under its own name.
    REQUIRE (session.undoNames().front() == "Record Take");

    const auto afterTheTake = session.stateDigest();

    REQUIRE (session.undo());
    REQUIRE (session.track (keys).clips.empty());
    REQUIRE (session.stateDigest() == beforeTheTake);

    REQUIRE (session.redo());
    REQUIRE (session.stateDigest() == afterTheTake);
}

TEST_CASE ("a recorded audio take is written into the project's audio subdirectory")
{
    const TempProject project;
    Session session { project.editFile() };
    session.useNoAudioDevice();

    const auto audioInput = inputOfKind (session, InputKind::audio);
    REQUIRE (audioInput != duet::model::noInput);

    TrackRef guitar = duet::model::noTrack;

    session.performAction (
        "Add a track", [&] (auto& ops) { guitar = ops.createTrack (TrackKind::audio, "Guitar"); });

    // A bare session is not a project, so it is told where takes go — the shape
    // of a project folder is the persistence facade's to know.
    session.setRecordingDirectory (duet::persistence::audioDirectory (project.folder()));

    session.setTrackInput (guitar, audioInput);
    session.setTrackRecordArmed (guitar, true);

    // Off the start of the timeline, so that where the take lands is a fact
    // about the engine's own latency compensation and not about zero.
    session.setPlaybackPositionSeconds (takeStartSeconds);

    const auto beforeTheTake = session.stateDigest();

    session.startRecording();
    session.runWithoutAudioDevice (2.0, { {}, 220.0, 0.5 });
    session.stopRecording();

    const auto clips = session.track (guitar).clips;
    REQUIRE (clips.size() == 1);

    const auto& take = clips.front();

    // The take is where the engine put it: Duet moves a recorded clip nowhere,
    // so latency compensation stays the engine's own.
    REQUIRE_THAT (take.startSeconds, WithinAbs (takeStartSeconds, 0.05));

    // The project travels with its recordings: the file is inside the project
    // folder's audio subdirectory, and the reference the project stores is
    // relative to the project folder and not to anywhere on this machine.
    REQUIRE (take.sourceReference.starts_with ("audio/"));
    REQUIRE (take.sourceFile.parent_path() == duet::persistence::audioDirectory (project.folder()));
    REQUIRE (std::filesystem::exists (take.sourceFile));
    REQUIRE (duet::testing::peakLevelOf (take.sourceFile) > 0.1);

    REQUIRE (session.undoNames().front() == "Record Take");

    const auto afterTheTake = session.stateDigest();

    REQUIRE (session.undo());
    REQUIRE (session.track (guitar).clips.empty());
    REQUIRE (session.stateDigest() == beforeTheTake);

    // The clip goes; the audio the producer played does not.
    REQUIRE (std::filesystem::exists (take.sourceFile));

    REQUIRE (session.redo());
    REQUIRE (session.stateDigest() == afterTheTake);
}

TEST_CASE ("a take is recorded into a brand new project that has never been saved")
{
    const TempProject scratch;

    // ADR 0005: a project is a folder from the moment it is created, so there is
    // somewhere for a take to be written before anything has been saved.
    const auto project = duet::persistence::Project::create (scratch.folder() / "Untitled");
    REQUIRE (project != nullptr);

    auto& session = project->session();
    session.useNoAudioDevice();

    const auto audioInput = inputOfKind (session, InputKind::audio);
    REQUIRE (audioInput != duet::model::noInput);

    TrackRef guitar = duet::model::noTrack;

    session.performAction (
        "Add a track", [&] (auto& ops) { guitar = ops.createTrack (TrackKind::audio, "Guitar"); });

    session.setTrackInput (guitar, audioInput);
    session.setTrackRecordArmed (guitar, true);

    session.startRecording();
    session.runWithoutAudioDevice (1.0, { {}, 220.0, 0.5 });
    session.stopRecording();

    const auto clips = session.track (guitar).clips;
    REQUIRE (clips.size() == 1);
    // Nothing told this session where takes go: opening the project did.
    REQUIRE (clips.front().sourceFile.parent_path()
             == duet::persistence::audioDirectory (project->folder()));
    REQUIRE (duet::testing::peakLevelOf (clips.front().sourceFile) > 0.1);

    // The take is a change to the project, and the project says so.
    REQUIRE (project->hasUnsavedChanges());
}

TEST_CASE ("a take started as the first transport gesture of a session survives the rebuild")
{
    const TempProject project;
    Session session { project.editFile() };

    if (session.audioDeviceDescription().empty())
        SKIP ("this machine has no audio device to record through");

    session.setRecordingDirectory (duet::persistence::audioDirectory (project.folder()));

    TrackRef guitar = duet::model::noTrack;

    session.performAction (
        "Add a track", [&] (auto& ops) { guitar = ops.createTrack (TrackKind::audio, "Guitar"); });

    // The producer cannot arm an input they have not been offered, and the
    // engine offers its wave inputs from the message loop, so this is the wait
    // the app does not have to do. It stays well inside the seconds before the
    // engine's own timer would rebuild the device list, which is what leaves
    // that rebuild ahead of the take below rather than behind it.
    for (int attempt = 0; attempt < inputAttempts && session.availableInputs().empty(); ++attempt)
        duet::testing::pumpMessages (msPerInputAttempt);

    const auto audioInput = inputOfKind (session, InputKind::audio);
    REQUIRE (audioInput != duet::model::noInput);

    session.setTrackInput (guitar, audioInput);
    session.setTrackRecordArmed (guitar, true);

    // Record as the first thing this session's transport is ever asked for,
    // which is where hazard 6 lands. startRecording asks for the rebuild
    // immediately, and the production quiet (100 ms) waits out both applies
    // before the take starts — a take begun between them is ended by the
    // second.
    session.startRecording();

    for (int attempt = 0; attempt < 20 && ! session.isRecording(); ++attempt)
        duet::testing::pumpMessages (20);

    REQUIRE (session.isRecording());
    duet::testing::pumpMessages (1000);

    session.stopRecording();
    REQUIRE_FALSE (session.isRecording());

    // One clip, from where Record was pressed: a take the rebuild had ended
    // would be no clip at all, or a clip that stopped where the rebuild landed.
    const auto clips = session.track (guitar).clips;
    REQUIRE (clips.size() == 1);
    REQUIRE_THAT (clips.front().startSeconds, WithinAbs (0.0, 0.05));
    REQUIRE (clips.front().lengthSeconds > 0.0);
}

TEST_CASE ("a take waiting for the engine's devices is stopped by a Stop")
{
    const TempProject project;
    Session session { project.editFile() };

    TrackRef guitar = duet::model::noTrack;

    session.performAction (
        "Add a track", [&] (auto& ops) { guitar = ops.createTrack (TrackKind::audio, "Guitar"); });

    // Asked for before the engine has its devices, so this take is waiting.
    session.startRecording();
    REQUIRE_FALSE (session.isRecording());

    session.useNoAudioDevice();
    session.setTrackInput (guitar, inputOfKind (session, InputKind::audio));
    session.setTrackRecordArmed (guitar, true);

    session.setDeviceWait (100, 1, 1000);

    // Stopping is the producer's last word, and it reaches a take that has been
    // asked for and has not begun: everything the waiting take needed to start
    // is in place, and it must not start anyway.
    session.stopPlayback();

    duet::testing::pumpMessages (20);

    REQUIRE_FALSE (session.isRecording());
    REQUIRE_FALSE (session.isPlaying());
    REQUIRE (session.track (guitar).clips.empty());
}
