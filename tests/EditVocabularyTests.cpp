#include <duet/model/Session.h>

#include <duet/persistence/ProjectLayout.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

#include <thread>
#include <type_traits>

using duet::model::ClipRef;
using duet::model::Session;
using duet::model::TrackRef;
using duet::testing::TempProject;

TEST_CASE ("an Action enters the undo history as one step under its own name")
{
    const TempProject project;
    Session session { project.editFile() };

    session.performAction ("Add a drum track", [] (auto& ops) { ops.addTrack ("Drums"); });

    REQUIRE (session.undoNames() == std::vector<std::string> { "Add a drum track" });
    REQUIRE (session.tracks().size() == 2);
    REQUIRE (session.tracks().back().name == "Drums");
}

TEST_CASE (
    "five operations in one Action are one undo step, and undo and redo return the exact state")
{
    const TempProject project;
    const auto loop = project.writeTone ("drum-loop.wav", 2.0, 220.0);
    Session session { project.editFile() };

    const auto before = session.stateDigest();

    session.performAction ("Add drum loop",
                           [&loop] (auto& ops)
                           {
                               const auto track = ops.addTrack ("Drums");

                               for (int bar = 0; bar < 4; ++bar)
                                   ops.insertAudioClip (
                                       track, "loop " + std::to_string (bar), loop, bar * 2.0, 2.0);
                           });

    const auto after = session.stateDigest();

    REQUIRE (session.undoNames() == std::vector<std::string> { "Add drum loop" });
    REQUIRE (session.tracks().back().clips.size() == 4);
    REQUIRE (after != before);

    REQUIRE (session.undo());
    REQUIRE (session.stateDigest() == before);
    REQUIRE (session.tracks().size() == 1);

    REQUIRE (session.redo());
    REQUIRE (session.stateDigest() == after);
    REQUIRE (session.tracks().back().clips.size() == 4);
}

TEST_CASE ("two Actions undo and redo back to the states they started from")
{
    const TempProject project;
    const auto tone = project.writeTone ("tone.wav", 1.0, 440.0);
    Session session { project.editFile() };

    const auto initial = session.stateDigest();

    session.performAction ("Add a bass track", [] (auto& ops) { ops.addTrack ("Bass"); });
    const auto afterFirst = session.stateDigest();

    session.performAction (
        "Add a bass clip",
        [&session, &tone] (auto& ops)
        { ops.insertAudioClip (session.tracks().back().track, "bass", tone, 0.0, 1.0); });
    const auto afterSecond = session.stateDigest();

    REQUIRE (session.undo());
    REQUIRE (session.undo());
    REQUIRE (session.stateDigest() == initial);
    REQUIRE_FALSE (session.undo());

    REQUIRE (session.redo());
    REQUIRE (session.stateDigest() == afterFirst);
    REQUIRE (session.redo());
    REQUIRE (session.stateDigest() == afterSecond);
    REQUIRE_FALSE (session.redo());
}

TEST_CASE ("the vocabulary covers the track and clip operations, each as one Action")
{
    const TempProject project;
    const auto tone = project.writeTone ("tone.wav", 4.0, 330.0);
    Session session { project.editFile() };

    TrackRef drums = duet::model::noTrack;
    TrackRef bass = duet::model::noTrack;
    ClipRef clip = duet::model::noClip;

    session.performAction ("Build a project",
                           [&] (auto& ops)
                           {
                               drums = ops.addTrack ("Drums");
                               bass = ops.addTrack ("Bass");
                               clip = ops.insertAudioClip (drums, "loop", tone, 1.0, 4.0);
                           });

    REQUIRE (session.tracks().size() == 3);
    REQUIRE (session.tracks()[1].name == "Drums");
    REQUIRE (session.tracks()[2].name == "Bass");

    SECTION ("a track is renamed")
    {
        session.performAction ("Rename the track",
                               [&] (auto& ops) { ops.renameTrack (drums, "Beats"); });

        REQUIRE (session.tracks()[1].name == "Beats");
        REQUIRE (session.undo());
        REQUIRE (session.tracks()[1].name == "Drums");
    }

    SECTION ("a track is moved to another place in the running order")
    {
        session.performAction ("Reorder the tracks", [&] (auto& ops) { ops.moveTrack (bass, 1); });

        REQUIRE (session.tracks()[1].name == "Bass");
        REQUIRE (session.tracks()[2].name == "Drums");
        REQUIRE (session.undo());
        REQUIRE (session.tracks()[1].name == "Drums");
    }

    SECTION ("a track is removed with everything on it")
    {
        session.performAction ("Remove the track", [&] (auto& ops) { ops.removeTrack (drums); });

        REQUIRE (session.tracks().size() == 2);
        REQUIRE (session.undo());
        REQUIRE (session.tracks().size() == 3);
        REQUIRE (session.tracks()[1].clips.size() == 1);
    }

    SECTION ("a clip is moved, trimmed and deleted")
    {
        session.performAction ("Move the clip", [&] (auto& ops) { ops.moveClip (clip, 8.0); });
        REQUIRE (session.tracks()[1].clips.front().startSeconds == 8.0);

        session.performAction ("Trim the clip", [&] (auto& ops) { ops.trimClip (clip, 2.0); });
        REQUIRE (session.tracks()[1].clips.front().lengthSeconds == 2.0);
        REQUIRE (session.tracks()[1].clips.front().startSeconds == 8.0);

        session.performAction ("Delete the clip", [&] (auto& ops) { ops.deleteClip (clip); });
        REQUIRE (session.tracks()[1].clips.empty());

        REQUIRE (session.undo());
        REQUIRE (session.tracks()[1].clips.size() == 1);
    }
}

TEST_CASE ("an inserted clip refers to its source inside the project, and plays it")
{
    const TempProject project;
    const auto tone = project.writeTone ("tone.wav", 2.0, 440.0);
    Session session { project.editFile() };

    session.performAction (
        "Insert the tone",
        [&] (auto& ops)
        { ops.insertAudioClip (session.tracks().front().track, "tone", tone, 0.0, 2.0); });

    const auto clip = session.tracks().front().clips.front();

    INFO ("stored source reference: " << clip.sourceReference);
    REQUIRE_FALSE (std::filesystem::path { clip.sourceReference }.is_absolute());
    REQUIRE (clip.sourceFile == tone);
    REQUIRE (clip.sourceFile.parent_path() == duet::persistence::audioDirectory (project.folder()));

    const auto rendered = project.folder() / "rendered.wav";

    REQUIRE (session.renderToFile (rendered));
    REQUIRE (duet::testing::peakLevelOf (rendered) > 0.1);
}

TEST_CASE ("the engine's deferred clip re-sort joins the Action that caused it")
{
    const TempProject project;
    const auto tone = project.writeTone ("tone.wav", 1.0, 440.0);
    Session session { project.editFile() };

    ClipRef first = duet::model::noClip;

    session.performAction ("Lay out two clips",
                           [&] (auto& ops)
                           {
                               const auto track = session.tracks().front().track;
                               first = ops.insertAudioClip (track, "first", tone, 0.0, 1.0);
                               ops.insertAudioClip (track, "second", tone, 4.0, 1.0);
                           });
    duet::testing::pumpMessages (400);

    const auto beforeMove = session.stateDigest();

    // Moving the first clip past the second is what makes the engine re-sort the
    // clip list, asynchronously and through the undo history.
    session.performAction ("Move the clip", [&] (auto& ops) { ops.moveClip (first, 8.0); });
    duet::testing::pumpMessages (400);

    const auto afterMove = session.stateDigest();

    // The re-sort has run when the clips have swapped places in the list.
    REQUIRE (session.tracks().front().clips.front().name == "second");
    REQUIRE (afterMove != beforeMove);
    REQUIRE (session.undoNames().size() == 2);
    REQUIRE (session.undoNames().front() == "Move the clip");

    REQUIRE (session.undo());
    REQUIRE (session.stateDigest() == beforeMove);
    REQUIRE (session.tracks().front().clips.front().name == "first");
    REQUIRE (session.redoNames() == std::vector<std::string> { "Move the clip" });

    REQUIRE (session.redo());
    REQUIRE (session.stateDigest() == afterMove);
}

TEST_CASE ("the undo history holds the newest two hundred Actions")
{
    const TempProject project;
    Session session { project.editFile() };
    const auto track = session.tracks().front().track;

    for (int take = 0; take <= 200; ++take)
        session.performAction ("Rename to take " + std::to_string (take),
                               [&] (auto& ops)
                               { ops.renameTrack (track, "Take " + std::to_string (take)); });

    REQUIRE (session.tracks().front().name == "Take 200");
    REQUIRE (session.undoNames().size() == 200);
    REQUIRE (session.undoNames().front() == "Rename to take 200");
    REQUIRE (session.undoNames().back() == "Rename to take 1");

    while (session.undo())
        ;

    // The two hundred newest Actions came back off the stack; the oldest one,
    // which named the track in the first place, is no longer undoable.
    REQUIRE (session.tracks().front().name == "Take 0");
}

TEST_CASE ("the operations cannot be reached outside an Action")
{
    using duet::model::EditOps;

    STATIC_REQUIRE_FALSE (std::is_default_constructible_v<EditOps>);
    STATIC_REQUIRE_FALSE (std::is_constructible_v<EditOps, Session&>);
    STATIC_REQUIRE_FALSE (std::is_copy_constructible_v<EditOps>);
    STATIC_REQUIRE_FALSE (std::is_move_constructible_v<EditOps>);

    // Nor can one be kept: only performAction, as its friend, can destroy one.
    STATIC_REQUIRE_FALSE (std::is_destructible_v<EditOps>);
}

TEST_CASE ("an Action off the message thread fails loudly and changes nothing")
{
    const TempProject project;
    Session session { project.editFile() };

    bool refused = false;

    std::thread worker { [&]
                         {
                             try
                             {
                                 session.performAction ("From a worker thread",
                                                        [] (auto& ops)
                                                        { ops.addTrack ("Should not exist"); });
                             }
                             catch (const std::logic_error&)
                             {
                                 refused = true;
                             }
                         } };
    worker.join();

    REQUIRE (refused);
    REQUIRE (session.tracks().size() == 1);
    REQUIRE (session.undoNames().empty());
}

TEST_CASE ("the state digest is stable across an undo and redo round trip")
{
    const TempProject project;
    const auto tone = project.writeTone ("tone.wav", 2.0, 440.0);
    Session session { project.editFile() };

    session.performAction (
        "Lay out the loop",
        [&] (auto& ops)
        { ops.insertAudioClip (session.tracks().front().track, "loop", tone, 0.0, 2.0); });

    const auto laidOut = session.stateDigest();

    session.performAction ("Trim the loop",
                           [&] (auto& ops)
                           { ops.trimClip (session.tracks().front().clips.front().clip, 1.0); });
    duet::testing::pumpMessages (400);

    const auto trimmed = session.stateDigest();

    for (int round = 0; round < 3; ++round)
    {
        REQUIRE (session.undo());
        REQUIRE (session.stateDigest() == laidOut);
        REQUIRE (session.redo());
        REQUIRE (session.stateDigest() == trimmed);
    }
}

TEST_CASE ("a headless session plays through the engine's one-time device rebuild")
{
    const TempProject project;
    const auto tone = project.writeTone ("tone.wav", 8.0, 440.0);
    Session session { project.editFile() };

    session.performAction (
        "Lay out the loop",
        [&] (auto& ops)
        { ops.insertAudioClip (session.tracks().front().track, "loop", tone, 0.0, 8.0); });

    if (session.audioDeviceDescription().empty())
        SKIP ("this machine has no audio device to play through");

    REQUIRE (duet::testing::playUntilRolling (session));

    // Hazard 6: seconds after the first headless playback the engine rebuilds
    // its device list, which frees the playback graph and stops the transport.
    duet::testing::pumpMessages (4000);

    REQUIRE (duet::testing::playUntilRolling (session));

    session.stopPlayback();
}
