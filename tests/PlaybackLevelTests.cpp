#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;

namespace
{
/** Loud enough that nobody would call it silence, and far enough below the
    level the demo phrase actually reaches that the assertion is about being
    heard and not about a gain staying put.
*/
constexpr double audibleDb = -40.0;

/** How much of the project each case plays. A second is several hundred blocks
    and covers the demo phrase's first notes with room to spare.
*/
constexpr double playedSeconds = 1.0;
} // namespace

/** What the playback graph puts out.

    These are the assertions the offline render cannot make. The engine builds
    one graph to play through and another to render, and the playing one is the
    one that wraps a track with nowhere to go in a node that blocks its audio —
    which is how a group bus that swallowed everything routed into it shipped
    through a green suite (ADR 0006).
*/
TEST_CASE ("a track routed through a group bus is heard at the output")
{
    const TempProject project;
    Session session { project.editFile() };
    session.loadDemoContent();

    const auto track = session.tracks().front().track;
    TrackRef bus = duet::model::noTrack;

    session.performAction ("Route into a group bus",
                           [&] (auto& ops)
                           {
                               bus = ops.createTrack (TrackKind::group, "Bus");
                               ops.setTrackOutput (track, bus);
                           });

    duet::testing::pumpMessages (200);

    REQUIRE (session.playWithoutAudioDevice (playedSeconds));

    // Read before asserting: a meter clears as it is read, so the INFO has to
    // hold the numbers the assertions are about.
    const auto trackPeak = session.trackPeakDb (track);
    const auto busPeak = session.trackPeakDb (bus);
    const auto outputPeak = session.outputPeakDb();

    INFO ("track " << trackPeak << " dB, bus " << busPeak << " dB, output " << outputPeak << " dB");

    REQUIRE (trackPeak > audibleDb);
    REQUIRE (busPeak > audibleDb);
    REQUIRE (outputPeak > audibleDb);
}

TEST_CASE ("muting a track silences it during playback")
{
    const TempProject project;
    Session session { project.editFile() };
    session.loadDemoContent();

    const auto track = session.tracks().front().track;
    session.performAction ("Mute Track", [track] (auto& ops) { ops.setTrackMute (track, true); });
    duet::testing::pumpMessages (200);

    REQUIRE (session.playWithoutAudioDevice (playedSeconds));
    REQUIRE (session.trackPeakDb (track) <= duet::model::silentDb);
    REQUIRE (session.outputPeakDb() <= duet::model::silentDb);
}

TEST_CASE ("the output says when a project has run out of headroom")
{
    const TempProject project;
    Session session { project.editFile() };
    session.loadDemoContent();

    const auto track = session.tracks().front().track;

    REQUIRE (session.playWithoutAudioDevice (playedSeconds));

    const auto asPlayed = session.outputPeakDb();
    INFO ("the demo phrase peaks at " << asPlayed << " dB");
    REQUIRE (asPlayed > audibleDb);
    REQUIRE (asPlayed < 0.0);

    // Full scale is 0 dB, and a project can be pushed past it the way any
    // producer pushes one past it: the fader as far up as it goes, and the same
    // signal a second time through a bus on top of that.
    session.performAction ("Pile the gain up",
                           [track] (auto& ops)
                           {
                               ops.setTrackVolumeDb (track, 6.0);
                               const auto bus = ops.createTrack (TrackKind::group, "Loud");
                               ops.setSend (track, bus, 6.0);
                               ops.setTrackVolumeDb (bus, 6.0);
                           });

    duet::testing::pumpMessages (200);

    REQUIRE (session.playWithoutAudioDevice (playedSeconds));

    const auto pushed = session.outputPeakDb();
    INFO ("piled up, the output peaks at " << pushed << " dB");

    REQUIRE (pushed >= 0.0);
}

TEST_CASE ("a project played through a real audio device is heard at the output")
{
    const TempProject project;
    Session session { project.editFile() };
    session.loadDemoContent();

    if (session.audioDeviceDescription().empty())
        SKIP ("this machine has no audio device to play through");

    REQUIRE (duet::testing::playUntilRolling (session));
    duet::testing::pumpMessages (1000);

    const auto outputPeak = session.outputPeakDb();
    INFO ("output " << outputPeak << " dB on " << session.audioDeviceDescription());

    REQUIRE (outputPeak > audibleDb);

    session.stopPlayback();
}
