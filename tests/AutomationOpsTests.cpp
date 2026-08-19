#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using duet::model::AutomationTarget;
using duet::model::PluginRef;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;

TEST_CASE ("automation points are put on a curve, moved, and taken off again")
{
    const TempProject project;
    Session session { project.editFile() };

    TrackRef keys = duet::model::noTrack;

    session.performAction ("Add a track",
                           [&] (auto& ops) { keys = ops.createTrack (TrackKind::audio, "Keys"); });

    const auto pan = AutomationTarget::trackPanOf (keys);
    const auto beforeCurve = session.stateDigest();

    REQUIRE (session.automationPoints (pan).empty());

    session.performAction ("Draw the pan",
                           [&] (auto& ops) { ops.setAutomationPoints (pan, { { 2.0, 0.75 } }); });

    const auto points = session.automationPoints (pan);

    REQUIRE (points.size() == 1);
    REQUIRE (points.front().timeSeconds == 2.0);
    REQUIRE (points.front().value == 0.75);

    SECTION ("undo takes the curve back to what it was")
    {
        REQUIRE (session.undoNames().front() == "Draw the pan");
        REQUIRE (session.undo());
        REQUIRE (session.stateDigest() == beforeCurve);
        REQUIRE (session.automationPoints (pan).empty());

        REQUIRE (session.redo());
        REQUIRE (session.automationPoints (pan).size() == 1);
    }

    SECTION ("a point at a time the curve already has takes the new value")
    {
        session.performAction ("Pan it the other way",
                               [&] (auto& ops)
                               { ops.setAutomationPoints (pan, { { 2.0, -0.25 } }); });

        REQUIRE (session.automationPoints (pan).size() == 1);
        REQUIRE (session.automationPoints (pan).front().value == -0.25);
    }

    SECTION ("a point moves by being taken off one time and put on another")
    {
        session.performAction ("Move the point",
                               [&] (auto& ops)
                               {
                                   ops.removeAutomationPoints (pan, 2.0, 2.0);
                                   ops.setAutomationPoints (pan, { { 6.0, 0.75 } });
                               });

        const auto moved = session.automationPoints (pan);

        REQUIRE (moved.size() == 1);
        REQUIRE (moved.front().timeSeconds == 6.0);
        REQUIRE (moved.front().value == 0.75);
    }

    SECTION ("a stretch of a curve is cleared in one Action")
    {
        session.performAction (
            "Draw the rest of the pan",
            [&] (auto& ops)
            { ops.setAutomationPoints (pan, { { 4.0, 0.5 }, { 6.0, 0.25 }, { 8.0, 0.0 } }); });

        REQUIRE (session.automationPoints (pan).size() == 4);

        const auto beforeClearing = session.stateDigest();

        session.performAction ("Clear the middle",
                               [&] (auto& ops) { ops.removeAutomationPoints (pan, 3.0, 7.0); });

        const auto remaining = session.automationPoints (pan);

        REQUIRE (remaining.size() == 2);
        REQUIRE (remaining.front().timeSeconds == 2.0);
        REQUIRE (remaining.back().timeSeconds == 8.0);

        REQUIRE (session.undo());
        REQUIRE (session.stateDigest() == beforeClearing);
        REQUIRE (session.automationPoints (pan).size() == 4);
    }
}

TEST_CASE ("a volume curve is written and read in decibels, like the fader itself")
{
    const TempProject project;
    Session session { project.editFile() };

    TrackRef keys = duet::model::noTrack;

    session.performAction ("Add a track",
                           [&] (auto& ops) { keys = ops.createTrack (TrackKind::audio, "Keys"); });

    const auto volume = AutomationTarget::trackVolumeOf (keys);

    session.performAction ("Fade the track in",
                           [&] (auto& ops)
                           { ops.setAutomationPoints (volume, { { 0.0, -30.0 }, { 4.0, 0.0 } }); });

    const auto points = session.automationPoints (volume);

    REQUIRE (points.size() == 2);
    REQUIRE_THAT (points.front().value, WithinAbs (-30.0, 0.001));
    REQUIRE_THAT (points.back().value, WithinAbs (0.0, 0.001));
}

TEST_CASE ("a plugin parameter carries a curve of its own")
{
    const TempProject project;
    Session session { project.editFile() };

    TrackRef bass = duet::model::noTrack;
    PluginRef compressor = duet::model::noPlugin;

    session.performAction ("Compress the bass",
                           [&] (auto& ops)
                           {
                               bass = ops.createTrack (TrackKind::audio, "Bass");
                               compressor =
                                   ops.addPlugin (bass, duet::model::BuiltinPlugin::compressor, 0);
                           });

    const auto ratio = AutomationTarget::parameterOf (compressor, "ratio");
    const auto beforeCurve = session.stateDigest();

    session.performAction ("Ride the ratio",
                           [&] (auto& ops)
                           { ops.setAutomationPoints (ratio, { { 1.0, 0.2 }, { 3.0, 0.8 } }); });

    const auto points = session.automationPoints (ratio);

    REQUIRE (points.size() == 2);
    REQUIRE_THAT (points.front().value, WithinAbs (0.2, 0.000001));
    REQUIRE_THAT (points.back().value, WithinAbs (0.8, 0.000001));

    REQUIRE (session.undo());
    REQUIRE (session.stateDigest() == beforeCurve);
    REQUIRE (session.automationPoints (ratio).empty());
}
