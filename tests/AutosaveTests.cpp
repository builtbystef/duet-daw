#include <duet/gui/AutosaveSettings.h>

#include <duet/persistence/ProjectLayout.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <fstream>
#include <sstream>

using duet::persistence::AutosaveInterval;
using duet::testing::StoredSettings;
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
} // namespace

TEST_CASE ("autosave is an app-global setting with four choices and a ten-minute default")
{
    StoredSettings settings;

    REQUIRE (duet::gui::autosaveInterval (settings) == AutosaveInterval::tenMinutes);

    for (const auto interval : { AutosaveInterval::off,
                                 AutosaveInterval::twoMinutes,
                                 AutosaveInterval::fiveMinutes,
                                 AutosaveInterval::tenMinutes })
    {
        duet::gui::setAutosaveInterval (settings, interval);
        REQUIRE (duet::gui::autosaveInterval (settings) == interval);
    }
}

TEST_CASE ("an elapsed autosave interval writes recovery without touching the project file")
{
    const TempProject temp;
    const auto folder = freshFolderIn (temp);
    const auto project = duet::persistence::Project::create (folder);
    REQUIRE (project != nullptr);

    const auto projectFile = duet::persistence::editFile (folder);
    const auto savedBytes = fileContents (projectFile);
    const auto savedTime = std::filesystem::last_write_time (projectFile);
    const auto start = std::chrono::steady_clock::time_point {};

    project->setAutosaveInterval (AutosaveInterval::twoMinutes, start);
    project->session().performAction ("Add a bass track",
                                      [] (auto& ops)
                                      { ops.createTrack (duet::model::TrackKind::audio, "Bass"); });

    REQUIRE_FALSE (project->autosaveTick (start + std::chrono::minutes { 2 }
                                          - std::chrono::milliseconds { 1 }));
    REQUIRE (project->autosaveTick (start + std::chrono::minutes { 2 }));
    REQUIRE (std::filesystem::is_regular_file (duet::persistence::recoveryFile (folder)));
    REQUIRE (fileContents (projectFile) == savedBytes);
    REQUIRE (std::filesystem::last_write_time (projectFile) == savedTime);
}

TEST_CASE ("clean and disabled projects write no recovery, and a changed interval applies now")
{
    const TempProject temp;
    const auto folder = freshFolderIn (temp);
    const auto project = duet::persistence::Project::create (folder);
    REQUIRE (project != nullptr);

    const auto start = std::chrono::steady_clock::time_point {};
    project->setAutosaveInterval (AutosaveInterval::twoMinutes, start);
    REQUIRE_FALSE (project->autosaveTick (start + std::chrono::minutes { 2 }));
    REQUIRE_FALSE (std::filesystem::exists (duet::persistence::recoveryFile (folder)));

    project->session().performAction ("Add a bass track",
                                      [] (auto& ops)
                                      { ops.createTrack (duet::model::TrackKind::audio, "Bass"); });
    project->setAutosaveInterval (AutosaveInterval::off, start);
    REQUIRE_FALSE (project->autosaveTick (start + std::chrono::hours { 24 }));
    REQUIRE_FALSE (std::filesystem::exists (duet::persistence::recoveryFile (folder)));

    const auto changedAt = start + std::chrono::hours { 24 };
    project->setAutosaveInterval (AutosaveInterval::twoMinutes, changedAt);
    REQUIRE (project->autosaveTick (changedAt + std::chrono::minutes { 2 }));
}

TEST_CASE ("repeated autosaves replace the one recovery snapshot")
{
    const TempProject temp;
    const auto folder = freshFolderIn (temp);
    const auto project = duet::persistence::Project::create (folder);
    REQUIRE (project != nullptr);

    const auto start = std::chrono::steady_clock::time_point {};
    project->setAutosaveInterval (AutosaveInterval::twoMinutes, start);
    project->session().performAction (
        "Add bass", [] (auto& ops) { ops.createTrack (duet::model::TrackKind::audio, "Bass"); });
    REQUIRE (project->autosaveTick (start + std::chrono::minutes { 2 }));
    const auto firstRecovery = fileContents (duet::persistence::recoveryFile (folder));

    project->session().performAction (
        "Add keys", [] (auto& ops) { ops.createTrack (duet::model::TrackKind::audio, "Keys"); });
    REQUIRE (project->autosaveTick (start + std::chrono::minutes { 4 }));

    REQUIRE (fileContents (duet::persistence::recoveryFile (folder)) != firstRecovery);
    REQUIRE_FALSE (std::filesystem::exists (duet::persistence::partialRecoveryFile (folder)));

    std::size_t recoveryFiles = 0;
    for (const auto& entry : std::filesystem::directory_iterator { folder })
        if (entry.path().filename().string().find ("recovery") != std::string::npos)
            ++recoveryFiles;

    REQUIRE (recoveryFiles == 1);
}

TEST_CASE ("a newer recovery can be restored or declined after the prior session disappears")
{
    const TempProject temp;
    const auto folder = freshFolderIn (temp);
    const auto start = std::chrono::steady_clock::time_point {};

    {
        const auto project = duet::persistence::Project::create (folder);
        REQUIRE (project != nullptr);
        project->session().performAction (
            "Add bass",
            [] (auto& ops) { ops.createTrack (duet::model::TrackKind::audio, "Bass"); });
        REQUIRE (project->save());

        project->setAutosaveInterval (AutosaveInterval::twoMinutes, start);
        project->session().performAction (
            "Add keys",
            [] (auto& ops) { ops.createTrack (duet::model::TrackKind::audio, "Keys"); });
        project->session().performAction (
            "Rename keys",
            [&] (auto& ops)
            { ops.renameTrack (project->session().tracks().back().track, "Synth"); });
        REQUIRE (project->session().undo());
        REQUIRE (project->autosaveTick (start + std::chrono::minutes { 2 }));
        REQUIRE (project->session().redoNames() == std::vector<std::string> { "Rename keys" });
    }

    REQUIRE (duet::persistence::Project::recoveryAvailable (folder));

    auto restored = duet::persistence::Project::openWithResult (
        folder, duet::persistence::RecoveryChoice::restore);
    REQUIRE (restored.project != nullptr);
    REQUIRE (restored.recoveryAvailable);
    REQUIRE (restored.project->hasUnsavedChanges());
    REQUIRE (restored.project->session().tracks().back().name == "Keys");
    restored.project.reset();

    auto declined = duet::persistence::Project::openWithResult (
        folder, duet::persistence::RecoveryChoice::decline);
    REQUIRE (declined.project != nullptr);
    REQUIRE (declined.recoveryAvailable);
    REQUIRE (declined.project->session().tracks().back().name == "Bass");
    REQUIRE_FALSE (duet::persistence::Project::recoveryAvailable (folder));
}

TEST_CASE ("an explicit save removes recovery so reopening offers none")
{
    const TempProject temp;
    const auto folder = freshFolderIn (temp);
    const auto project = duet::persistence::Project::create (folder);
    REQUIRE (project != nullptr);

    const auto start = std::chrono::steady_clock::time_point {};
    project->setAutosaveInterval (AutosaveInterval::twoMinutes, start);
    project->session().performAction (
        "Add bass", [] (auto& ops) { ops.createTrack (duet::model::TrackKind::audio, "Bass"); });
    REQUIRE (project->autosaveTick (start + std::chrono::minutes { 2 }));
    REQUIRE (project->save());
    REQUIRE_FALSE (std::filesystem::exists (duet::persistence::recoveryFile (folder)));

    const auto reopened = duet::persistence::Project::openWithResult (folder);
    REQUIRE (reopened.project != nullptr);
    REQUIRE_FALSE (reopened.recoveryAvailable);
}
