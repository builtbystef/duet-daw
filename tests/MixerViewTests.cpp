#include <duet/gui/Mixer.h>
#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>

using Catch::Matchers::WithinAbs;
using duet::gui::Mixer;
using duet::model::Session;
using duet::model::TrackKind;
using duet::testing::TempProject;

TEST_CASE ("the mixer follows track order and ends with its specialised master strip")
{
    const TempProject project;
    Session session { project.editFile() };
    duet::model::TrackRef first = duet::model::noTrack;
    duet::model::TrackRef second = duet::model::noTrack;
    session.performAction ("Tracks",
                           [&] (auto& ops)
                           {
                               first = ops.createTrack (TrackKind::audio, "First");
                               second = ops.createTrack (TrackKind::midi, "Second");
                           });
    Mixer mixer;
    mixer.setSession (&session);

    auto strips = mixer.strips();
    REQUIRE (strips.size() == static_cast<std::size_t> (session.audioTrackCount() + 1));
    REQUIRE (strips[strips.size() - 3].channel == first);
    REQUIRE (strips[strips.size() - 3].name == "First");
    REQUIRE (strips[strips.size() - 2].channel == second);
    REQUIRE (strips.back().channel == duet::model::masterChannel);
    REQUIRE (strips.back().name == "Master");
    REQUIRE_FALSE (strips.back().colour.has_value());
    REQUIRE_FALSE (strips.back().canSolo);
    REQUIRE_FALSE (strips.back().canRoute);

    session.performAction ("Reorder", [&] (auto& ops) { ops.moveTrack (second, 0); });
    strips = mixer.strips();
    REQUIRE (strips[0].channel == second);
    REQUIRE (strips.back().channel == duet::model::masterChannel);
    const auto firstStrip = std::find_if (
        strips.begin(), strips.end(), [first] (const auto& item) { return item.channel == first; });
    REQUIRE (firstStrip != strips.end());
    REQUIRE (firstStrip > strips.begin());
}

TEST_CASE ("a mixer fader gesture previews transiently and commits one Action")
{
    const TempProject project;
    Session session { project.editFile() };
    duet::model::TrackRef track = duet::model::noTrack;
    session.performAction (
        "Track", [&] (auto& ops) { track = ops.createTrack (TrackKind::audio, "Audio"); });
    const auto actionsBefore = session.undoNames().size();
    Mixer mixer;
    mixer.setSession (&session);

    mixer.beginFaderGesture (track);
    mixer.dragFaderTo (track, -6.0);
    REQUIRE_THAT (mixer.strip (track).volumeDb, WithinAbs (-6.0, 0.001));
    REQUIRE_THAT (session.liveTrackVolumeDb (track), WithinAbs (-6.0, 0.001));
    REQUIRE (session.undoNames().size() == actionsBefore);
    mixer.endFaderGesture (track);

    REQUIRE_THAT (session.track (track).volumeDb, WithinAbs (-6.0, 0.001));
    REQUIRE (session.undoNames().front() == "Set Track Fader");
    REQUIRE (session.undoNames().size() == actionsBefore + 1);
    REQUIRE (session.undo());
    REQUIRE_THAT (session.track (track).volumeDb, WithinAbs (0.0, 0.001));

    mixer.resetFader (track);
    REQUIRE_THAT (session.track (track).volumeDb, WithinAbs (0.0, 0.001));
}

TEST_CASE ("cancelling a mixer gesture restores the original with no Action")
{
    const TempProject project;
    Session session { project.editFile() };
    duet::model::TrackRef track = duet::model::noTrack;
    session.performAction (
        "Track", [&] (auto& ops) { track = ops.createTrack (TrackKind::audio, "Audio"); });
    Mixer mixer;
    mixer.setSession (&session);
    const auto actionsBefore = session.undoNames();

    mixer.beginPanGesture (track);
    mixer.dragPanTo (track, 0.75);
    mixer.cancelGesture();

    REQUIRE_THAT (session.track (track).pan, WithinAbs (0.0, 0.000001));
    REQUIRE (session.undoNames() == actionsBefore);
}

TEST_CASE ("mixer routing offers only cycle-safe group destinations")
{
    const TempProject project;
    Session session { project.editFile() };
    duet::model::TrackRef audio = duet::model::noTrack;
    duet::model::TrackRef firstGroup = duet::model::noTrack;
    duet::model::TrackRef secondGroup = duet::model::noTrack;
    session.performAction ("Tracks",
                           [&] (auto& ops)
                           {
                               audio = ops.createTrack (TrackKind::audio, "Audio");
                               firstGroup = ops.createTrack (TrackKind::group, "One");
                               secondGroup = ops.createTrack (TrackKind::group, "Two");
                               ops.setTrackOutput (firstGroup, secondGroup);
                           });
    Mixer mixer;
    mixer.setSession (&session);

    const auto audioRoutes = mixer.routingDestinations (audio);
    REQUIRE (audioRoutes.size() == 3);
    const auto groupRoutes = mixer.routingDestinations (secondGroup);
    REQUIRE (groupRoutes.size() == 1); // Main Output; routing to One would cycle.

    mixer.setOutput (audio, firstGroup);
    REQUIRE (session.track (audio).output == firstGroup);
    REQUIRE (session.undoNames().front() == "Set Track Output");
}

TEST_CASE ("insert bypass and reorder gestures each commit exactly once")
{
    const TempProject project;
    Session session { project.editFile() };
    duet::model::TrackRef track = duet::model::noTrack;
    session.performAction (
        "Track", [&] (auto& ops) { track = ops.createTrack (TrackKind::audio, "Audio"); });
    Mixer mixer;
    mixer.setSession (&session);

    const auto eq = mixer.addBuiltin (track, duet::model::BuiltinPlugin::eq, 0);
    const auto compressor = mixer.addBuiltin (track, duet::model::BuiltinPlugin::compressor, 1);
    REQUIRE (session.track (track).plugins.size() == 2);

    mixer.toggleBypass (eq);
    REQUIRE (session.track (track).plugins[0].bypassed);
    REQUIRE (session.undoNames().front() == "Bypass Plugin");

    mixer.reorderPlugin (compressor, 0);
    REQUIRE (session.track (track).plugins[0].plugin == compressor);
    REQUIRE (session.undoNames().front() == "Reorder Plugin");
}

TEST_CASE ("meter sampling touches only visible strips and holds then falls deterministically")
{
    const TempProject project;
    Session session { project.editFile() };
    session.performAction ("Sixty tracks",
                           [&] (auto& ops)
                           {
                               for (int index = 0; index < 60; ++index)
                                   ops.createTrack (TrackKind::audio, "Track");
                           });
    Mixer mixer;
    mixer.setSession (&session);
    mixer.setVisibleRange (10, 4);

    REQUIRE (mixer.sampleMeters (0.0) == 4);
    REQUIRE (mixer.lastSampledChannels().size() == 4);
    REQUIRE_THAT (mixer.strip (mixer.strips()[10].channel).meterDb,
                  WithinAbs (duet::model::silentDb, 0.001));

    // The deterministic decay seam does not depend on a wall-clock timer.
    mixer.observeMeterPeakForTesting (mixer.strips()[10].channel, -6.0, 0.0);
    mixer.sampleMeters (0.75);
    REQUIRE_THAT (mixer.strip (mixer.strips()[10].channel).meterDb, WithinAbs (-6.0, 0.001));
    mixer.sampleMeters (1.5);
    REQUIRE_THAT (mixer.strip (mixer.strips()[10].channel).meterDb, WithinAbs (-18.0, 0.001));
}
