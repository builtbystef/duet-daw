#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>

using Catch::Matchers::WithinAbs;
using duet::model::BuiltinPlugin;
using duet::model::PluginRef;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;

namespace
{
/** Where a plugin sits in a track's chain, or −1 when it is not in it. */
int positionOf (const Session& session, TrackRef track, PluginRef plugin)
{
    const auto chain = session.track (track).plugins;

    for (std::size_t position = 0; position < chain.size(); ++position)
        if (chain[position].plugin == plugin)
            return static_cast<int> (position);

    return -1;
}

double parameterOf (const Session& session, PluginRef plugin, const std::string& parameterId)
{
    for (const auto& parameter : session.pluginParameters (plugin))
        if (parameter.parameterId == parameterId)
            return parameter.value;

    return 0.0;
}
} // namespace

TEST_CASE ("a built-in plugin is added at a chain position, reordered, and removed")
{
    const TempProject project;
    Session session { project.editFile() };

    TrackRef keys = duet::model::noTrack;

    session.performAction ("Add a track",
                           [&] (auto& ops) { keys = ops.createTrack (TrackKind::audio, "Keys"); });

    const auto emptyChain = session.track (keys).plugins.size();
    const auto beforeChain = session.stateDigest();

    PluginRef eq = duet::model::noPlugin;
    PluginRef reverb = duet::model::noPlugin;

    session.performAction ("Add the effects",
                           [&] (auto& ops)
                           {
                               eq = ops.addPlugin (keys, BuiltinPlugin::eq, 0);
                               reverb = ops.addPlugin (keys, BuiltinPlugin::reverb, 1);
                           });

    REQUIRE (session.track (keys).plugins.size() == emptyChain + 2);
    REQUIRE (positionOf (session, keys, eq) == 0);
    REQUIRE (positionOf (session, keys, reverb) == 1);
    REQUIRE (session.track (keys).plugins.front().builtin == BuiltinPlugin::eq);

    SECTION ("reordering moves it, and undo puts it back")
    {
        const auto beforeReorder = session.stateDigest();

        session.performAction ("Move the reverb first",
                               [&] (auto& ops) { ops.reorderPlugin (reverb, 0); });

        REQUIRE (positionOf (session, keys, reverb) == 0);
        REQUIRE (positionOf (session, keys, eq) == 1);

        REQUIRE (session.undo());
        REQUIRE (session.stateDigest() == beforeReorder);
        REQUIRE (positionOf (session, keys, eq) == 0);
    }

    SECTION ("removing takes it out, and undo restores the chain exactly")
    {
        const auto beforeRemoval = session.stateDigest();

        session.performAction ("Take the eq out", [&] (auto& ops) { ops.removePlugin (eq); });

        REQUIRE (positionOf (session, keys, eq) == -1);
        REQUIRE (session.track (keys).plugins.size() == emptyChain + 1);

        REQUIRE (session.undo());
        REQUIRE (session.stateDigest() == beforeRemoval);
        REQUIRE (positionOf (session, keys, eq) == 0);
    }

    SECTION ("undoing the whole Action leaves the chain as it was")
    {
        REQUIRE (session.undo());
        REQUIRE (session.stateDigest() == beforeChain);
        REQUIRE (session.track (keys).plugins.size() == emptyChain);
    }
}

TEST_CASE ("a compressor takes a parameter and a sidechain source from another track")
{
    const TempProject project;
    Session session { project.editFile() };

    TrackRef bass = duet::model::noTrack;
    TrackRef kick = duet::model::noTrack;
    PluginRef compressor = duet::model::noPlugin;

    session.performAction ("Sidechain the bass to the kick",
                           [&] (auto& ops)
                           {
                               kick = ops.createTrack (TrackKind::audio, "Kick");
                               bass = ops.createTrack (TrackKind::audio, "Bass");
                               compressor = ops.addPlugin (bass, BuiltinPlugin::compressor, 0);
                               ops.setPluginSidechainSource (compressor, kick);
                               ops.setPluginParameter (compressor, "ratio", 0.25);
                           });

    // One element of a Suggestion, one Action: the whole thing reads back.
    REQUIRE (session.undoNames() == std::vector<std::string> { "Sidechain the bass to the kick" });
    REQUIRE (session.track (bass).plugins.front().builtin == BuiltinPlugin::compressor);
    REQUIRE (session.track (bass).plugins.front().sidechainSource == kick);
    REQUIRE_THAT (parameterOf (session, compressor, "ratio"), WithinAbs (0.25, 0.000001));

    const auto parameters = session.pluginParameters (compressor);

    REQUIRE (parameters.size() > 1);
    REQUIRE (std::any_of (parameters.begin(),
                          parameters.end(),
                          [] (const auto& parameter)
                          { return parameter.parameterId == "threshold"; }));

    SECTION ("a later parameter change is its own Action and undoes to the value before it")
    {
        session.performAction ("Loosen the ratio",
                               [&] (auto& ops)
                               { ops.setPluginParameter (compressor, "ratio", 0.75); });

        REQUIRE_THAT (parameterOf (session, compressor, "ratio"), WithinAbs (0.75, 0.000001));

        REQUIRE (session.undo());
        REQUIRE_THAT (parameterOf (session, compressor, "ratio"), WithinAbs (0.25, 0.000001));

        REQUIRE (session.redo());
        REQUIRE_THAT (parameterOf (session, compressor, "ratio"), WithinAbs (0.75, 0.000001));
    }

    SECTION ("the sidechain source is cleared")
    {
        session.performAction (
            "Unhook the sidechain",
            [&] (auto& ops) { ops.setPluginSidechainSource (compressor, duet::model::noTrack); });

        REQUIRE (session.track (bass).plugins.front().sidechainSource == duet::model::noTrack);
    }
}

TEST_CASE ("a chain position counts the producer's plugins, not the ones Duet put there")
{
    const TempProject project;
    const auto tone = project.writeTone ("stab.wav", 1.0, 440.0);
    Session session { project.editFile() };

    TrackRef source = duet::model::noTrack;
    TrackRef reverbBus = duet::model::noTrack;

    session.performAction ("Send a stab into a bus",
                           [&] (auto& ops)
                           {
                               source = ops.createTrack (TrackKind::audio, "Stab");
                               ops.insertAudioClip (source, "stab", tone, 0.0, 1.0);

                               // Somewhere for the render to put the tail, as the
                               // reverb-send test in MixerOpsTests explains.
                               const auto room = ops.createTrack (TrackKind::midi, "Room to ring");
                               ops.insertMidiClip (room, "Room", 0.0, 3.0);

                               reverbBus = ops.createTrack (TrackKind::group, "Reverb");
                               ops.setSend (source, reverbBus, 0.0);
                           });

    // The send put a return, and every track is born with a fader and a meter.
    // The producer asked for none of them, so neither chain has anything in it.
    REQUIRE (session.track (reverbBus).plugins.empty());
    REQUIRE (session.track (source).plugins.empty());

    PluginRef eq = duet::model::noPlugin;
    PluginRef reverb = duet::model::noPlugin;

    session.performAction ("Add the effects",
                           [&] (auto& ops)
                           {
                               eq = ops.addPlugin (reverbBus, BuiltinPlugin::eq, 0);
                               reverb = ops.addPlugin (reverbBus, BuiltinPlugin::reverb, 1);
                               ops.setPluginParameter (reverb, "room size", 0.9);
                               ops.setPluginParameter (reverb, "wet level", 1.0);
                               ops.setPluginParameter (reverb, "dry level", 0.0);
                           });

    REQUIRE (session.track (reverbBus).plugins.size() == 2);
    REQUIRE (positionOf (session, reverbBus, eq) == 0);
    REQUIRE (positionOf (session, reverbBus, reverb) == 1);

    session.performAction ("Put the reverb first",
                           [&] (auto& ops) { ops.reorderPlugin (reverb, 0); });

    REQUIRE (positionOf (session, reverbBus, reverb) == 0);
    REQUIRE (positionOf (session, reverbBus, eq) == 1);

    // The same position zero as addPlugin's: first among the producer's effects
    // and still behind the return, so the bus is heard rather than silent.
    const auto rendered = project.folder() / "reordered.wav";

    REQUIRE (session.renderToFile (rendered));

    const auto tail = duet::testing::peakLevelBetween (rendered, 1.3, 2.5);

    INFO ("tail level: " << tail);
    REQUIRE (tail > 0.01);
}
