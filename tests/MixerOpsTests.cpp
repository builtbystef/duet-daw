#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;

namespace
{
/** The fader stores a position, not a level, and does it in single precision,
    so a level goes out and comes back within a small fraction of a decibel.
*/
constexpr double decibelTolerance = 0.001;
} // namespace

TEST_CASE ("the mixer values of a track are set, read back, and undone exactly")
{
    const TempProject project;
    Session session { project.editFile() };

    TrackRef keys = duet::model::noTrack;

    session.performAction ("Add a track",
                           [&] (auto& ops) { keys = ops.createTrack (TrackKind::audio, "Keys"); });

    const auto beforeMixing = session.stateDigest();

    SECTION ("the fader")
    {
        session.performAction ("Set the fader",
                               [&] (auto& ops) { ops.setTrackVolumeDb (keys, -6.0); });

        REQUIRE_THAT (session.track (keys).volumeDb, WithinAbs (-6.0, decibelTolerance));

        REQUIRE (session.undoNames().front() == "Set the fader");
        REQUIRE (session.undo());
        REQUIRE (session.stateDigest() == beforeMixing);
    }

    SECTION ("the pan")
    {
        session.performAction ("Pan the track", [&] (auto& ops) { ops.setTrackPan (keys, -0.5); });

        REQUIRE_THAT (session.track (keys).pan, WithinAbs (-0.5, 0.000001));

        REQUIRE (session.undo());
        REQUIRE (session.stateDigest() == beforeMixing);
        REQUIRE_THAT (session.track (keys).pan, WithinAbs (0.0, 0.000001));
    }

    SECTION ("mute and solo")
    {
        session.performAction ("Mute the track",
                               [&] (auto& ops) { ops.setTrackMute (keys, true); });
        REQUIRE (session.track (keys).muted);

        session.performAction ("Solo the track",
                               [&] (auto& ops) { ops.setTrackSolo (keys, true); });
        REQUIRE (session.track (keys).soloed);
        REQUIRE (session.track (keys).muted);

        REQUIRE (session.undo());
        REQUIRE_FALSE (session.track (keys).soloed);

        REQUIRE (session.undo());
        REQUIRE_FALSE (session.track (keys).muted);
        REQUIRE (session.stateDigest() == beforeMixing);
    }
}

TEST_CASE ("undoing a fader move moves the fader, and leaves the redo stack alone")
{
    const TempProject project;
    Session session { project.editFile() };

    TrackRef keys = duet::model::noTrack;

    session.performAction ("Add a track",
                           [&] (auto& ops) { keys = ops.createTrack (TrackKind::audio, "Keys"); });

    session.performAction ("Pull the fader down",
                           [&] (auto& ops) { ops.setTrackVolumeDb (keys, -40.0); });

    REQUIRE_THAT (session.liveTrackVolumeDb (keys), WithinAbs (-40.0, decibelTolerance));

    REQUIRE (session.undo());

    // The engine keeps a plugin's value in the plugin as well as in the state
    // and does not follow a state change back into the plugin, so without the
    // model putting the two back in step this fader would still be pulled down
    // and the undo would be inaudible.
    REQUIRE_THAT (session.liveTrackVolumeDb (keys), WithinAbs (0.0, decibelTolerance));

    // And putting them back in step must not itself be a change: a change after
    // an undo is what clears the redo stack.
    REQUIRE (session.redoNames() == std::vector<std::string> { "Pull the fader down" });
    REQUIRE (session.redo());
    REQUIRE_THAT (session.liveTrackVolumeDb (keys), WithinAbs (-40.0, decibelTolerance));
}

TEST_CASE ("a send into a group bus is set at a level, read back, and undone exactly")
{
    const TempProject project;
    Session session { project.editFile() };

    TrackRef keys = duet::model::noTrack;
    TrackRef reverbBus = duet::model::noTrack;

    session.performAction ("Lay out the tracks",
                           [&] (auto& ops)
                           {
                               keys = ops.createTrack (TrackKind::audio, "Keys");
                               reverbBus = ops.createTrack (TrackKind::group, "Reverb");
                           });

    REQUIRE (session.track (keys).sends.empty());

    const auto beforeSending = session.stateDigest();

    session.performAction ("Send the keys to the reverb",
                           [&] (auto& ops) { ops.setSend (keys, reverbBus, -12.0); });

    const auto sends = session.track (keys).sends;

    REQUIRE (sends.size() == 1);
    REQUIRE (sends.front().bus == reverbBus);
    REQUIRE_THAT (sends.front().levelDb, WithinAbs (-12.0, decibelTolerance));

    SECTION ("the same send is set again rather than made twice")
    {
        session.performAction ("Turn the send down",
                               [&] (auto& ops) { ops.setSend (keys, reverbBus, -24.0); });

        REQUIRE (session.track (keys).sends.size() == 1);
        REQUIRE_THAT (session.track (keys).sends.front().levelDb,
                      WithinAbs (-24.0, decibelTolerance));
    }

    SECTION ("the whole send undoes in one step")
    {
        REQUIRE (session.undoNames().front() == "Send the keys to the reverb");
        REQUIRE (session.undo());
        REQUIRE (session.stateDigest() == beforeSending);
        REQUIRE (session.track (keys).sends.empty());
    }
}

TEST_CASE ("a send into a reverb bus is heard, and rings on after the source stops")
{
    const TempProject project;
    const auto tone = project.writeTone ("stab.wav", 1.0, 440.0);
    Session session { project.editFile() };

    duet::model::TrackRef source = duet::model::noTrack;
    duet::model::TrackRef reverbBus = duet::model::noTrack;

    session.performAction ("Send a stab into a reverb",
                           [&] (auto& ops)
                           {
                               source = ops.createTrack (TrackKind::audio, "Stab");
                               ops.insertAudioClip (source, "stab", tone, 0.0, 1.0);

                               // A render runs for as long as the edit does, and
                               // a tail that outlives every clip would be cut off
                               // at the last one. This empty clip is what gives
                               // the render somewhere to put it.
                               const auto room = ops.createTrack (TrackKind::midi, "Room to ring");
                               ops.insertMidiClip (room, "Room", 0.0, 3.0);

                               reverbBus = ops.createTrack (TrackKind::group, "Reverb");
                               ops.setSend (source, reverbBus, 0.0);

                               // Position 1, after the return setSend puts at the
                               // head of the bus. A reverb in front of the return
                               // sits upstream of the only thing feeding it, so it
                               // processes silence and is never heard — which is
                               // exactly what this test is here to catch.
                               const auto reverb =
                                   ops.addPlugin (reverbBus, duet::model::BuiltinPlugin::reverb, 1);
                               ops.setPluginParameter (reverb, "room size", 0.9);
                               ops.setPluginParameter (reverb, "wet level", 1.0);
                               ops.setPluginParameter (reverb, "dry level", 0.0);
                           });

    const auto rendered = project.folder() / "with-reverb.wav";

    REQUIRE (session.renderToFile (rendered));

    // The source is one second long, so anything after it is the bus ringing on.
    const auto tail = duet::testing::peakLevelBetween (rendered, 1.3, 2.5);

    INFO ("tail level: " << tail);
    REQUIRE (tail > 0.01);
}
