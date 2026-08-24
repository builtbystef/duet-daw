#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

using duet::model::BuiltinPlugin;
using duet::model::Session;
using duet::model::TrackColour;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;

TEST_CASE ("a track is created of each kind, and a midi track with an instrument")
{
    const TempProject project;
    Session session { project.editFile() };

    TrackRef audio = duet::model::noTrack;
    TrackRef keys = duet::model::noTrack;
    TrackRef drums = duet::model::noTrack;
    TrackRef bus = duet::model::noTrack;

    session.performAction (
        "Lay out the tracks",
        [&] (auto& ops)
        {
            audio = ops.createTrack (TrackKind::audio, "Vocals");
            keys = ops.createTrack (TrackKind::midi, "Keys", BuiltinPlugin::synth);
            drums = ops.createTrack (TrackKind::midi, "Drums", BuiltinPlugin::sampler);
            bus = ops.createTrack (TrackKind::group, "Bus");
        });

    REQUIRE (session.track (audio).kind == TrackKind::audio);
    REQUIRE (session.track (keys).kind == TrackKind::midi);
    REQUIRE (session.track (drums).kind == TrackKind::midi);
    REQUIRE (session.track (bus).kind == TrackKind::group);

    REQUIRE (session.track (audio).name == "Vocals");

    // The instrument goes at the head of the chain, in front of the fader that
    // every track starts with.
    const auto chain = session.track (keys).plugins;

    REQUIRE_FALSE (chain.empty());
    REQUIRE (chain.front().builtin == BuiltinPlugin::synth);
    REQUIRE (session.track (drums).plugins.front().builtin == BuiltinPlugin::sampler);

    // A track asked for without an instrument gets none.
    for (const auto& plugin : session.track (audio).plugins)
        REQUIRE (plugin.builtin != BuiltinPlugin::synth);
}

TEST_CASE ("a track's output is routed into a group bus, and the routing undoes exactly")
{
    const TempProject project;
    Session session { project.editFile() };

    TrackRef vocals = duet::model::noTrack;
    TrackRef bus = duet::model::noTrack;

    session.performAction ("Lay out the tracks",
                           [&] (auto& ops)
                           {
                               vocals = ops.createTrack (TrackKind::audio, "Vocals");
                               bus = ops.createTrack (TrackKind::group, "Vocal bus");
                           });

    REQUIRE (session.track (vocals).output == duet::model::noTrack);

    const auto beforeRouting = session.stateDigest();

    session.performAction ("Route the vocals into the bus",
                           [&] (auto& ops) { ops.setTrackOutput (vocals, bus); });

    REQUIRE (session.track (vocals).output == bus);

    REQUIRE (session.undoNames().front() == "Route the vocals into the bus");
    REQUIRE (session.undo());
    REQUIRE (session.stateDigest() == beforeRouting);
    REQUIRE (session.track (vocals).output == duet::model::noTrack);

    REQUIRE (session.redo());
    REQUIRE (session.track (vocals).output == bus);

    // No bus: the track goes to the default output again.
    constexpr auto noBus = duet::model::noTrack;

    session.performAction ("Send the vocals out again",
                           [&] (auto& ops) { ops.setTrackOutput (vocals, noBus); });

    REQUIRE (session.track (vocals).output == duet::model::noTrack);
}

TEST_CASE ("a track colour is project state and undoes exactly")
{
    const TempProject project;
    Session session { project.editFile() };

    TrackRef keys = duet::model::noTrack;
    session.performAction ("Add MIDI Track",
                           [&] (auto& ops) { keys = ops.createTrack (TrackKind::midi, "Keys"); });

    const auto beforeColour = session.stateDigest();
    session.performAction ("Set Track Colour",
                           [&] (auto& ops) { ops.setTrackColour (keys, TrackColour::blue); });

    REQUIRE (session.track (keys).colour == TrackColour::blue);
    REQUIRE (session.undoNames().front() == "Set Track Colour");
    REQUIRE (session.undo());
    REQUIRE (session.stateDigest() == beforeColour);
    REQUIRE (session.redo());
    REQUIRE (session.track (keys).colour == TrackColour::blue);
}

TEST_CASE ("duplicating a track copies its clips and producer plugin chain in one Action")
{
    const TempProject project;
    Session session { project.editFile() };

    TrackRef keys = duet::model::noTrack;
    duet::model::ClipRef phrase = duet::model::noClip;
    session.performAction ("Build Keys",
                           [&] (auto& ops)
                           {
                               keys =
                                   ops.createTrack (TrackKind::midi, "Keys", BuiltinPlugin::synth);
                               phrase = ops.insertMidiClip (keys, "Phrase", 1.0, 4.0);
                               ops.addNote (phrase, 60, 0.0, 1.0, 100);
                               ops.addPlugin (keys, BuiltinPlugin::reverb, 1);
                               ops.setTrackColour (keys, TrackColour::purple);
                           });

    const auto beforeDuplicate = session.stateDigest();
    const auto trackCountBefore = session.tracks().size();
    TrackRef copy = duet::model::noTrack;
    session.performAction ("Duplicate Track",
                           [&] (auto& ops) { copy = ops.duplicateTrack (keys); });

    REQUIRE (copy != duet::model::noTrack);
    REQUIRE (session.tracks().size() == trackCountBefore + 1);
    REQUIRE (session.track (copy).clips.size() == 1);
    REQUIRE (session.track (copy).plugins.size() == 2);
    REQUIRE (session.track (copy).colour == TrackColour::purple);
    REQUIRE (session.notes (session.track (copy).clips.front().clip).size() == 1);
    REQUIRE (session.undoNames().front() == "Duplicate Track");

    REQUIRE (session.undo());
    REQUIRE (session.stateDigest() == beforeDuplicate);
    REQUIRE (session.redo());
    REQUIRE (session.tracks().size() == trackCountBefore + 1);
}

TEST_CASE ("audio routed through a group bus still reaches the output")
{
    const TempProject project;
    const auto tone = project.writeTone ("tone.wav", 2.0, 440.0);
    Session session { project.editFile() };

    session.performAction ("Route the vocals through a bus",
                           [&] (auto& ops)
                           {
                               const auto vocals = ops.createTrack (TrackKind::audio, "Vocals");
                               const auto bus = ops.createTrack (TrackKind::group, "Vocal bus");
                               ops.insertAudioClip (vocals, "tone", tone, 0.0, 2.0);
                               ops.setTrackOutput (vocals, bus);
                           });

    const auto rendered = project.folder() / "through-the-bus.wav";

    REQUIRE (session.renderToFile (rendered));
    REQUIRE (duet::testing::peakLevelOf (rendered) > 0.1);
}
