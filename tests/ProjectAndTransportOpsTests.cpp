#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <functional>
#include <string>
#include <utility>
#include <vector>

using Catch::Matchers::WithinAbs;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;

TEST_CASE ("the tempo and the time signature are set as Actions, and undo to what they were")
{
    const TempProject project;
    Session session { project.editFile() };

    REQUIRE_THAT (session.tempoBpm(), WithinAbs (120.0, 0.000001));
    REQUIRE (session.timeSignature().numerator == 4);
    REQUIRE (session.timeSignature().denominator == 4);

    const auto beforeCounting = session.stateDigest();

    session.performAction ("Count it in six eight",
                           [] (auto& ops)
                           {
                               ops.setTempo (128.0);
                               ops.setTimeSignature (6, 8);
                           });

    REQUIRE_THAT (session.tempoBpm(), WithinAbs (128.0, 0.000001));
    REQUIRE (session.timeSignature().numerator == 6);
    REQUIRE (session.timeSignature().denominator == 8);

    REQUIRE (session.undoNames() == std::vector<std::string> { "Count it in six eight" });
    REQUIRE (session.undo());
    REQUIRE (session.stateDigest() == beforeCounting);
    REQUIRE_THAT (session.tempoBpm(), WithinAbs (120.0, 0.000001));
    REQUIRE (session.timeSignature().numerator == 4);
    REQUIRE (session.timeSignature().denominator == 4);

    REQUIRE (session.redo());
    REQUIRE_THAT (session.tempoBpm(), WithinAbs (128.0, 0.000001));
    REQUIRE (session.timeSignature().numerator == 6);
}

TEST_CASE ("a bar starts where the tempo and the time signature put it")
{
    const TempProject project;
    Session session { project.editFile() };

    // Four beats to the bar at 120 bpm: half a second a beat, two seconds a bar.
    REQUIRE_THAT (session.barStartSeconds (1), WithinAbs (0.0, 0.000001));
    REQUIRE_THAT (session.barStartSeconds (5), WithinAbs (8.0, 0.000001));

    session.performAction ("Double the tempo", [] (auto& ops) { ops.setTempo (240.0); });

    REQUIRE_THAT (session.barStartSeconds (5), WithinAbs (4.0, 0.000001));
}

TEST_CASE ("an undo cannot move the playhead")
{
    const TempProject project;
    Session session { project.editFile() };

    TrackRef keys = duet::model::noTrack;

    session.performAction ("Add a track",
                           [&] (auto& ops) { keys = ops.createTrack (TrackKind::audio, "Keys"); });

    session.setPlaybackPositionSeconds (12.0);
    REQUIRE_THAT (session.playbackPositionSeconds(), WithinAbs (12.0, 0.001));

    session.performAction ("Rename the track", [&] (auto& ops) { ops.renameTrack (keys, "Pads"); });

    REQUIRE (session.undo());

    // The transport is written with no undo history at all, so there is nothing
    // in an Action for an undo to take the playhead back to.
    REQUIRE_THAT (session.playbackPositionSeconds(), WithinAbs (12.0, 0.001));
    REQUIRE (session.tracks().back().name == "Keys");

    // Nor does the playhead appear in what a state comparison sees, so moving it
    // is not a change to the project.
    const auto beforeMoving = session.stateDigest();
    session.setPlaybackPositionSeconds (3.0);
    REQUIRE (session.stateDigest() == beforeMoving);
}

TEST_CASE ("one Action from every domain lands while the transport rolls")
{
    const TempProject project;
    const auto tone = project.writeTone ("tone.wav", 30.0, 220.0);
    Session session { project.editFile() };

    TrackRef keys = duet::model::noTrack;
    duet::model::ClipRef loop = duet::model::noClip;
    duet::model::ClipRef phrase = duet::model::noClip;
    TrackRef bus = duet::model::noTrack;
    duet::model::PluginRef reverb = duet::model::noPlugin;

    session.performAction (
        "Lay out the project",
        [&] (auto& ops)
        {
            keys = ops.createTrack (TrackKind::midi, "Keys", duet::model::BuiltinPlugin::synth);
            loop = ops.insertAudioClip (session.tracks().front().track, "loop", tone, 0.0, 30.0);
            phrase = ops.insertMidiClip (keys, "Phrase", 0.0, 8.0);
        });

    if (session.audioDeviceDescription().empty())
        SKIP ("this machine has no audio device to play through");

    REQUIRE (duet::testing::playUntilRolling (session));

    // Hazard 6, got out of the way first: the engine rebuilds its device list
    // once, seconds after the first playback, and that stops the transport. It
    // has to happen before the edits do, or a stop it caused would read as a
    // stop one of them caused.
    duet::testing::pumpMessages (5000);
    REQUIRE (duet::testing::playUntilRolling (session));
    duet::testing::pumpMessages (500);

    // One Action per domain, with the message loop running between them, which
    // is what lets the engine rebuild its playback graph from each of them.
    const std::vector<std::pair<std::string, std::function<void (duet::model::EditOps&)>>> domains {
        { "Add notes",
          [&] (auto& ops)
          {
              for (int note = 0; note < 8; ++note)
                  ops.addNote (phrase, 48 + note, note * 1.0, 0.9, 90);
          } },
        { "Loop and duplicate",
          [&] (auto& ops)
          {
              ops.setClipLoop (loop, true, 8.0);
              ops.duplicateClip (phrase, duet::model::noTrack, session.barStartSeconds (5));
          } },
        { "Route into a bus",
          [&] (auto& ops)
          {
              bus = ops.createTrack (TrackKind::group, "Bus");
              ops.setTrackOutput (keys, bus);
          } },
        { "Set the mixer",
          [&] (auto& ops)
          {
              ops.setTrackVolumeDb (keys, -6.0);
              ops.setTrackPan (keys, -0.4);
              ops.setSend (keys, bus, -12.0);
          } },
        { "Add a reverb",
          [&] (auto& ops)
          {
              reverb = ops.addPlugin (bus, duet::model::BuiltinPlugin::reverb, 0);
              ops.setPluginParameter (reverb, "room size", 0.9);
          } },
        { "Draw a curve",
          [&] (auto& ops)
          {
              ops.setAutomationPoints (duet::model::AutomationTarget::trackVolumeOf (keys),
                                       { { 0.0, -24.0 }, { 4.0, 0.0 } });
          } },
        { "Change the tempo",
          [] (auto& ops)
          {
              ops.setTempo (140.0);
              ops.setTimeSignature (6, 8);
          } },
    };

    for (const auto& [name, ops] : domains)
    {
        session.performAction (name, ops);

        // The first pump is what lets the Action reach the playback graph. Only
        // then is the playhead read, and read again, because the comparison has
        // to be of two positions the same Action is behind: a tempo change moves
        // the playhead in seconds by design — the beat it is on stays where it
        // is, and a faster tempo puts that beat earlier — and that move lands
        // with the rest of the Action.
        duet::testing::pumpMessages (300);

        INFO ("after: " << name);
        REQUIRE (session.isPlaying());

        const auto landed = session.playbackPositionSeconds();
        duet::testing::pumpMessages (300);

        REQUIRE (session.isPlaying());
        REQUIRE (session.playbackPositionSeconds() > landed);
    }

    REQUIRE (session.undoNames().size() == domains.size() + 1);

    session.stopPlayback();
}

TEST_CASE ("an undo cannot stop the transport")
{
    const TempProject project;
    const auto tone = project.writeTone ("tone.wav", 30.0, 440.0);
    Session session { project.editFile() };

    session.performAction (
        "Lay out the loop",
        [&] (auto& ops)
        { ops.insertAudioClip (session.tracks().front().track, "loop", tone, 0.0, 30.0); });

    if (session.audioDeviceDescription().empty())
        SKIP ("this machine has no audio device to play through");

    REQUIRE (duet::testing::playUntilRolling (session));

    session.setPlaybackPositionSeconds (5.0);
    duet::testing::pumpMessages (200);

    const auto positionBeforeUndo = session.playbackPositionSeconds();

    REQUIRE (session.undo());
    duet::testing::pumpMessages (200);

    REQUIRE (session.isPlaying());
    REQUIRE (session.playbackPositionSeconds() >= positionBeforeUndo);

    session.stopPlayback();
    REQUIRE_FALSE (session.isPlaying());
}
