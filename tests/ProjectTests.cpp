#include <duet/persistence/Project.h>

#include <duet/persistence/ProjectLayout.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <fstream>

using Catch::Matchers::WithinAbs;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::persistence::Project;
using duet::testing::TempProject;

namespace
{
/** A folder that holds no project yet, inside a temporary one that is cleaned
    up: what "create a project" is asked to make out of nothing.
*/
std::filesystem::path freshFolderIn (const TempProject& temp) { return temp.folder() / "Nocturne"; }
} // namespace

TEST_CASE ("a created project is a folder with an edit file and an audio subdirectory")
{
    const TempProject temp;
    const auto folder = freshFolderIn (temp);

    const auto project = Project::create (folder);

    REQUIRE (project != nullptr);
    REQUIRE (project->folder() == folder);
    REQUIRE (std::filesystem::is_directory (folder));
    REQUIRE (std::filesystem::is_regular_file (duet::persistence::editFile (folder)));
    REQUIRE (std::filesystem::is_directory (duet::persistence::audioDirectory (folder)));
}

TEST_CASE ("a project comes back from disk in the state it was saved in")
{
    const TempProject temp;
    const auto folder = freshFolderIn (temp);
    const auto tone = temp.writeTone ("tone.wav", 2.0, 440.0);

    std::string saved;

    {
        const auto project = Project::create (folder);
        REQUIRE (project != nullptr);

        const auto imported = project->importAudioFile (tone);

        project->session().performAction (
            "Lay out the loop",
            [&] (auto& ops)
            {
                const auto track = ops.createTrack (TrackKind::audio, "Keys");
                ops.insertAudioClip (track, "loop", imported, 1.0, 2.0);
            });

        saved = project->session().stateDigest();
        REQUIRE (project->save());
    }

    const auto reopened = Project::open (folder);

    REQUIRE (reopened != nullptr);
    REQUIRE (reopened->session().stateDigest() == saved);
    REQUIRE (reopened->session().tracks().back().name == "Keys");
    REQUIRE (reopened->session().tracks().back().clips.front().name == "loop");
}

TEST_CASE ("a project has nothing to open where no project was created")
{
    const TempProject temp;

    REQUIRE (Project::open (freshFolderIn (temp)) == nullptr);
}

TEST_CASE ("creating a project where one already lives refuses rather than overwrites")
{
    const TempProject temp;
    const auto folder = freshFolderIn (temp);

    const auto first = Project::create (folder);
    REQUIRE (first != nullptr);

    REQUIRE (Project::create (folder) == nullptr);
}

TEST_CASE ("an imported file lives in the project, and the project still plays it once moved")
{
    const TempProject temp;
    const auto folder = freshFolderIn (temp);

    // Written outside the project: what an import is, on the way in.
    const auto outside = temp.writeTone ("outside.wav", 2.0, 440.0);

    {
        const auto project = Project::create (folder);
        REQUIRE (project != nullptr);

        const auto imported = project->importAudioFile (outside);

        REQUIRE (imported == duet::persistence::audioDirectory (folder) / "outside.wav");
        REQUIRE (std::filesystem::is_regular_file (imported));

        project->session().performAction (
            "Import the tone",
            [&] (auto& ops) {
                ops.insertAudioClip (
                    project->session().tracks().front().track, "tone", imported, 0.0, 2.0);
            });

        const auto clip = project->session().tracks().front().clips.front();

        INFO ("stored source reference: " << clip.sourceReference);
        REQUIRE (clip.sourceReference == "audio/outside.wav");

        REQUIRE (project->save());
    }

    const auto moved = temp.folder() / "Nocturne (moved)";
    std::filesystem::rename (folder, moved);

    const auto reopened = Project::open (moved);
    REQUIRE (reopened != nullptr);

    const auto clip = reopened->session().tracks().front().clips.front();
    REQUIRE (clip.sourceFile == duet::persistence::audioDirectory (moved) / "outside.wav");

    const auto rendered = temp.folder() / "rendered.wav";
    REQUIRE (reopened->session().renderToFile (rendered));
    REQUIRE (duet::testing::peakLevelOf (rendered) > 0.1);
}

TEST_CASE ("a save brings back the fader the producer set, and leaves the redo stack alone")
{
    const TempProject temp;
    const auto folder = freshFolderIn (temp);
    const auto tone = temp.writeTone ("tone.wav", 4.0, 440.0);
    constexpr double faderDb = -6.0;

    {
        const auto project = Project::create (folder);
        REQUIRE (project != nullptr);
        auto& session = project->session();

        const auto imported = project->importAudioFile (tone);
        TrackRef track = duet::model::noTrack;

        session.performAction ("Set the fader",
                               [&] (auto& ops)
                               {
                                   track = session.tracks().front().track;
                                   ops.insertAudioClip (track, "tone", imported, 0.0, 4.0);
                                   ops.setTrackVolumeDb (track, faderDb);

                                   // A curve that is nowhere near the explicit value, so that
                                   // following it is unmistakable.
                                   ops.setAutomationPoints (
                                       duet::model::AutomationTarget::trackVolumeOf (track),
                                       { { 0.0, -30.0 }, { 4.0, 0.0 } });
                               });

        session.performAction ("Rename the track",
                               [&] (auto& ops) { ops.renameTrack (track, "Keys"); });

        REQUIRE (session.undo());
        REQUIRE (session.redoNames() == std::vector<std::string> { "Rename the track" });

        // Playing the project is what hands the fader to its curve: from here on
        // the fader is somewhere the producer never put it.
        REQUIRE (session.renderToFile (temp.folder() / "diverged.wav"));

        const auto explicitDb = session.track (track).volumeDb;

        INFO ("explicit " << explicitDb << " dB, live " << session.liveTrackVolumeDb (track)
                          << " dB");
        REQUIRE (session.liveTrackVolumeDb (track) != explicitDb);

        REQUIRE (project->save());

        // Hazard 3: the engine's own save would have written the fader's blob
        // through the undo history and taken this with it.
        REQUIRE (session.redoNames() == std::vector<std::string> { "Rename the track" });
        REQUIRE (session.redo());
        REQUIRE (session.tracks().front().name == "Keys");
    }

    const auto reopened = Project::open (folder);
    REQUIRE (reopened != nullptr);

    const auto track = reopened->session().tracks().front().track;
    REQUIRE_THAT (reopened->session().track (track).volumeDb, WithinAbs (faderDb, 0.001));
}

TEST_CASE ("Duet's own data and the refs it keys on survive a save and reload")
{
    const TempProject temp;
    const auto folder = freshFolderIn (temp);

    TrackRef bass = duet::model::noTrack;

    {
        const auto project = Project::create (folder);
        REQUIRE (project != nullptr);

        project->session().performAction ("Add a bass track",
                                          [&] (auto& ops)
                                          { bass = ops.createTrack (TrackKind::audio, "Bass"); });

        project->setDuetValue ("anchorTrack", std::to_string (bass));
        project->setDuetValue ("sessionNotes", "the anchor of the low end");

        REQUIRE (project->save());
    }

    const auto reopened = Project::open (folder);
    REQUIRE (reopened != nullptr);

    REQUIRE (reopened->duetValue ("sessionNotes") == "the anchor of the low end");
    REQUIRE (reopened->duetValue ("anchorTrack") == std::to_string (bass));

    const auto reloadedTrack = reopened->session().tracks().back();
    REQUIRE (reloadedTrack.name == "Bass");
    REQUIRE (reloadedTrack.track == bass);
}

TEST_CASE ("Duet's own data is not a producer edit, so undo does not take it back")
{
    const TempProject temp;
    const auto project = Project::create (freshFolderIn (temp));
    REQUIRE (project != nullptr);

    project->session().performAction (
        "Add a bass track", [] (auto& ops) { ops.createTrack (TrackKind::audio, "Bass"); });
    project->setDuetValue ("sessionNotes", "written while the track was there");

    REQUIRE (project->session().undo());
    REQUIRE (project->session().tracks().size() == 1);
    REQUIRE (project->duetValue ("sessionNotes") == "written while the track was there");
}

TEST_CASE ("a project is marked unsaved by an Action and cleared by a save")
{
    const TempProject temp;
    const auto project = Project::create (freshFolderIn (temp));
    REQUIRE (project != nullptr);

    // Creating the project wrote it, so there is nothing unsaved yet.
    REQUIRE_FALSE (project->hasUnsavedChanges());

    project->session().performAction (
        "Add a bass track", [] (auto& ops) { ops.createTrack (TrackKind::audio, "Bass"); });
    REQUIRE (project->hasUnsavedChanges());

    REQUIRE (project->save());
    REQUIRE_FALSE (project->hasUnsavedChanges());

    // Undoing back to what was saved is a change like any other: what the flag
    // reports is that the session has moved, not that the file is out of date.
    REQUIRE (project->session().undo());
    REQUIRE (project->hasUnsavedChanges());
}

TEST_CASE ("a save leaves no half-written file behind it")
{
    const TempProject temp;
    const auto folder = freshFolderIn (temp);
    const auto project = Project::create (folder);
    REQUIRE (project != nullptr);

    project->session().performAction (
        "Add a bass track", [] (auto& ops) { ops.createTrack (TrackKind::audio, "Bass"); });

    REQUIRE (project->save());
    REQUIRE_FALSE (std::filesystem::exists (duet::persistence::partialSaveFile (folder)));
}

TEST_CASE ("a save killed halfway leaves its own file behind, never a half-written project")
{
    const TempProject temp;
    const auto folder = freshFolderIn (temp);

    std::string saved;

    {
        const auto project = Project::create (folder);
        REQUIRE (project != nullptr);

        project->session().performAction (
            "Add a bass track", [] (auto& ops) { ops.createTrack (TrackKind::audio, "Bass"); });
        REQUIRE (project->save());
        saved = project->session().stateDigest();
    }

    // What a save killed between its write and its rename leaves on disk: half a
    // project, in the file the save writes to and never in the project's own.
    {
        std::ofstream halfWritten { duet::persistence::partialSaveFile (folder) };
        halfWritten << "<?xml version=\"1.0\"?>\n<EDIT appVersion=\"Unkno";
    }

    const auto reopened = Project::open (folder);

    REQUIRE (reopened != nullptr);
    REQUIRE (reopened->session().stateDigest() == saved);
    REQUIRE (reopened->session().tracks().back().name == "Bass");

    // And the next save clears the wreckage rather than tripping over it.
    REQUIRE (reopened->save());
    REQUIRE_FALSE (std::filesystem::exists (duet::persistence::partialSaveFile (folder)));
}

TEST_CASE ("a save that cannot be written leaves the last saved project intact")
{
    const TempProject temp;
    const auto folder = freshFolderIn (temp);
    const auto blocked = duet::persistence::partialSaveFile (folder);

    std::string saved;

    {
        const auto project = Project::create (folder);
        REQUIRE (project != nullptr);

        project->session().performAction (
            "Add a bass track", [] (auto& ops) { ops.createTrack (TrackKind::audio, "Bass"); });
        REQUIRE (project->save());
        saved = project->session().stateDigest();

        // A save writes beside the project file and renames the result onto it.
        // A directory that cannot be moved aside is that write failing — the one
        // moment the project file would be at risk if the save wrote it directly.
        std::filesystem::create_directories (blocked / "in the way");

        project->session().performAction (
            "Add a keys track", [] (auto& ops) { ops.createTrack (TrackKind::audio, "Keys"); });

        REQUIRE_FALSE (project->save());
        REQUIRE (project->hasUnsavedChanges());
    }

    std::filesystem::remove_all (blocked);

    const auto reopened = Project::open (folder);
    REQUIRE (reopened != nullptr);
    REQUIRE (reopened->session().stateDigest() == saved);
    REQUIRE (reopened->session().tracks().back().name == "Bass");
}
