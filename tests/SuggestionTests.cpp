#include <duet/model/Session.h>

#include <duet/persistence/Project.h>
#include <duet/persistence/ProjectLayout.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <fstream>
#include <sstream>

using duet::model::Session;
using duet::model::Suggestion;
using duet::model::TrackKind;
using duet::persistence::AutosaveInterval;
using duet::persistence::Project;
using duet::testing::TempProject;

namespace
{
std::filesystem::path freshFolderIn (const TempProject& temp) { return temp.folder() / "Nocturne"; }

std::string fileContents (const std::filesystem::path& file)
{
    const std::ifstream input { file };
    std::ostringstream contents;
    contents << input.rdbuf();
    return std::move (contents).str();
}

Suggestion addSubBassSuggestion()
{
    Suggestion suggestion { "Add sub bass" };
    const auto bass = suggestion.createTrack (TrackKind::audio, "Bass");
    suggestion.renameTrack (bass, "Sub Bass");
    return suggestion;
}
} // namespace

TEST_CASE ("a Suggestion operation can target a track created by an earlier operation")
{
    const TempProject project;
    Session session { project.editFile() };
    const auto suggestion = addSubBassSuggestion();

    REQUIRE (session.auditionSuggestion (suggestion));
    REQUIRE (session.tracks().back().name == "Sub Bass");

    session.stopAudition();
    REQUIRE (session.tracks().size() == 1);

    REQUIRE (session.acceptSuggestion (suggestion));
    REQUIRE (session.tracks().back().name == "Sub Bass");
}

TEST_CASE ("placeholder refs chain through created tracks clips and notes")
{
    const TempProject project;
    Session session { project.editFile() };
    Suggestion suggestion { "Add a bass phrase" };

    const auto track =
        suggestion.createTrack (TrackKind::midi, "Bass", duet::model::BuiltinPlugin::synth);
    const auto clip = suggestion.insertMidiClip (track, "Phrase", 0.0, 2.0);
    const auto note = suggestion.addNote (clip, 36, 0.0, 1.0, 80);
    suggestion.setNoteVelocity (note, 110);
    suggestion.setTrackVolumeDb (track, -6.0);

    const auto before = session.stateDigest();
    REQUIRE (session.auditionSuggestion (suggestion));
    REQUIRE (session.tracks().back().clips.size() == 1);
    REQUIRE (session.notes (session.tracks().back().clips.front().clip).front().velocity == 110);

    session.stopAudition();
    REQUIRE (session.stateDigest() == before);

    REQUIRE (session.acceptSuggestion (suggestion));
    REQUIRE (session.notes (session.tracks().back().clips.front().clip).front().velocity == 110);
}

TEST_CASE ("Audition changes the model and reverts digest-exact without writing the project file")
{
    const TempProject temp;
    const auto folder = freshFolderIn (temp);
    const auto project = Project::create (folder);
    REQUIRE (project != nullptr);

    const auto before = project->session().stateDigest();
    const auto saved = fileContents (duet::persistence::editFile (folder));
    const auto suggestion = addSubBassSuggestion();

    REQUIRE (project->session().auditionSuggestion (suggestion));
    REQUIRE (project->session().stateDigest() != before);
    REQUIRE (project->session().tracks().back().name == "Sub Bass");
    REQUIRE (fileContents (duet::persistence::editFile (folder)) == saved);

    project->session().stopAudition();
    REQUIRE (project->session().stateDigest() == before);
    REQUIRE (fileContents (duet::persistence::editFile (folder)) == saved);
}

TEST_CASE ("repeated A/B cycles leave no project or undo trace")
{
    const TempProject project;
    Session session { project.editFile() };

    session.performAction ("Name the first track",
                           [&] (auto& ops)
                           { ops.renameTrack (session.tracks().front().track, "Drums"); });
    session.performAction ("Name it again",
                           [&] (auto& ops)
                           { ops.renameTrack (session.tracks().front().track, "Beats"); });
    REQUIRE (session.undo());

    const auto before = session.stateDigest();
    const auto undoBefore = session.undoNames();
    const auto redoBefore = session.redoNames();
    const auto suggestion = addSubBassSuggestion();

    REQUIRE (session.auditionSuggestion (suggestion));

    for (int toggle = 0; toggle < 5; ++toggle)
    {
        if (session.isAuditioning())
            session.stopAudition();
        else
            REQUIRE (session.auditionSuggestion (suggestion));
    }

    REQUIRE_FALSE (session.isAuditioning());
    REQUIRE (session.stateDigest() == before);
    REQUIRE (session.undoNames() == undoBefore);
    REQUIRE (session.redoNames() == redoBefore);
}

TEST_CASE ("accepting a Suggestion is one named undo step with exact undo and redo")
{
    const TempProject project;
    Session session { project.editFile() };
    const auto before = session.stateDigest();
    const auto suggestion = addSubBassSuggestion();

    REQUIRE (session.auditionSuggestion (suggestion));
    REQUIRE (session.acceptSuggestion (suggestion));
    const auto accepted = session.stateDigest();

    REQUIRE (session.undoNames() == std::vector<std::string> { "Add sub bass" });
    REQUIRE (session.undo());
    REQUIRE (session.stateDigest() == before);
    REQUIRE (session.redo());
    REQUIRE (session.stateDigest() == accepted);
    REQUIRE (session.tracks().back().name == "Sub Bass");
}

TEST_CASE ("rejecting a pending or live Suggestion leaves project undo and redo unchanged")
{
    const TempProject project;
    Session session { project.editFile() };
    session.performAction ("Name the track",
                           [&] (auto& ops)
                           { ops.renameTrack (session.tracks().front().track, "Drums"); });
    session.performAction ("Rename the track",
                           [&] (auto& ops)
                           { ops.renameTrack (session.tracks().front().track, "Beats"); });
    REQUIRE (session.undo());

    const auto before = session.stateDigest();
    const auto undoBefore = session.undoNames();
    const auto redoBefore = session.redoNames();
    const auto idle = addSubBassSuggestion();

    session.rejectSuggestion (idle);
    REQUIRE (session.stateDigest() == before);
    REQUIRE (session.undoNames() == undoBefore);
    REQUIRE (session.redoNames() == redoBefore);

    const auto live = addSubBassSuggestion();
    REQUIRE (session.auditionSuggestion (live));
    session.rejectSuggestion (live);
    REQUIRE (session.stateDigest() == before);
    REQUIRE (session.undoNames() == undoBefore);
    REQUIRE (session.redoNames() == redoBefore);
}

TEST_CASE ("an explicit save during Audition reverts first and saves no suggested changes")
{
    const TempProject temp;
    const auto folder = freshFolderIn (temp);
    const auto project = Project::create (folder);
    REQUIRE (project != nullptr);

    const auto before = project->session().stateDigest();
    const auto suggestion = addSubBassSuggestion();
    REQUIRE (project->session().auditionSuggestion (suggestion));

    REQUIRE (project->save());
    REQUIRE_FALSE (project->session().isAuditioning());
    REQUIRE (project->session().stateDigest() == before);

    const auto reopened = Project::open (folder);
    REQUIRE (reopened != nullptr);
    REQUIRE (reopened->session().stateDigest() == before);
    REQUIRE (reopened->session().tracks().size() == 1);
}

TEST_CASE ("autosave skips a live Audition tick and resumes on the next interval")
{
    const TempProject temp;
    const auto folder = freshFolderIn (temp);
    const auto project = Project::create (folder);
    REQUIRE (project != nullptr);

    const auto start = std::chrono::steady_clock::time_point {};
    project->setAutosaveInterval (AutosaveInterval::twoMinutes, start);
    project->session().performAction (
        "Name the track",
        [&] (auto& ops) { ops.renameTrack (project->session().tracks().front().track, "Drums"); });

    const auto suggestion = addSubBassSuggestion();
    REQUIRE (project->session().auditionSuggestion (suggestion));
    REQUIRE_FALSE (project->autosaveTick (start + std::chrono::minutes { 2 }));
    REQUIRE_FALSE (std::filesystem::exists (duet::persistence::recoveryFile (folder)));

    project->session().stopAudition();
    REQUIRE_FALSE (project->autosaveTick (start + std::chrono::minutes { 4 }
                                          - std::chrono::milliseconds { 1 }));
    REQUIRE (project->autosaveTick (start + std::chrono::minutes { 4 }));
}

TEST_CASE ("a pending Suggestion is not saved and does not block explicit save")
{
    const TempProject temp;
    const auto folder = freshFolderIn (temp);
    const auto project = Project::create (folder);
    REQUIRE (project != nullptr);

    const auto before = project->session().stateDigest();
    const auto suggestion = addSubBassSuggestion();
    REQUIRE (suggestion.name() == "Add sub bass");
    REQUIRE (project->save());

    const auto reopened = Project::open (folder);
    REQUIRE (reopened != nullptr);
    REQUIRE (reopened->session().stateDigest() == before);
    REQUIRE (reopened->session().tracks().size() == 1);
}

TEST_CASE ("Audition enters, A/Bs and reverts without stopping a rolling transport")
{
    const TempProject project;
    Session session { project.editFile() };
    session.useNoAudioDevice();
    session.startPlayback();
    REQUIRE (session.isPlaying());

    const auto suggestion = addSubBassSuggestion();
    REQUIRE (session.auditionSuggestion (suggestion));
    REQUIRE (session.isPlaying());
    session.runWithoutAudioDevice (0.05);

    session.stopAudition();
    REQUIRE (session.isPlaying());
    REQUIRE (session.auditionSuggestion (suggestion));
    REQUIRE (session.isPlaying());

    session.stopAudition();
    REQUIRE (session.isPlaying());
    session.stopPlayback();
}
