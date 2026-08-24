#include <duet/app/ProjectLifecycle.h>

#include <duet/gui/AutosaveSettings.h>
#include <duet/gui/ProjectsSettings.h>
#include <duet/gui/ViewState.h>
#include <duet/persistence/ProjectLayout.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

using duet::app::ProjectLifecycle;
using duet::app::UnsavedDecision;
using duet::model::BuiltinPlugin;
using duet::model::TrackKind;
using duet::testing::StoredSettings;
using duet::testing::TempProject;

TEST_CASE ("launch without a prior project creates a numbered untitled project on disk")
{
    const TempProject temp;
    StoredSettings settings;
    ProjectLifecycle lifecycle { settings, temp.folder() / "Projects" };

    REQUIRE (lifecycle.launch());

    const auto& project = lifecycle.project();
    const auto expectedFolder = temp.folder() / "Projects" / "Untitled 1";
    REQUIRE (project.folder() == expectedFolder);
    REQUIRE (lifecycle.projectName() == "Untitled 1");
    REQUIRE (std::filesystem::is_regular_file (duet::persistence::editFile (expectedFolder)));
    REQUIRE (std::filesystem::is_directory (duet::persistence::audioDirectory (expectedFolder)));

    const auto tracks = project.session().tracks();
    REQUIRE (tracks.size() == 2);
    REQUIRE (tracks.at (0).kind == TrackKind::midi);
    REQUIRE (tracks.at (0).plugins.size() == 1);
    REQUIRE (tracks.at (0).plugins.front().builtin == BuiltinPlugin::synth);
    REQUIRE (tracks.at (1).kind == TrackKind::audio);
}

TEST_CASE ("untitled numbering never overwrites an existing folder")
{
    const TempProject temp;
    StoredSettings settings;
    const auto projects = temp.folder() / "Projects";
    std::filesystem::create_directories (projects / "Untitled 1");
    ProjectLifecycle lifecycle { settings, projects };

    REQUIRE (lifecycle.launch());
    REQUIRE (lifecycle.projectName() == "Untitled 2");
    REQUIRE (std::filesystem::is_directory (projects / "Untitled 1"));
}

TEST_CASE ("launch reopens the last project and its view, or replaces a missing one quietly")
{
    const TempProject temp;
    StoredSettings settings;
    const auto projects = temp.folder() / "Projects";
    std::filesystem::path firstFolder;

    {
        ProjectLifecycle firstLaunch { settings, projects };
        REQUIRE (firstLaunch.launch());
        firstFolder = firstLaunch.project().folder();

        duet::gui::ViewState view;
        view.setBrowserWidthPx (277);
        firstLaunch.project().onCaptureViewState ([&view] { return view.toData(); });
        REQUIRE (firstLaunch.save());
    }

    {
        ProjectLifecycle secondLaunch { settings, projects };
        REQUIRE (secondLaunch.launch());
        REQUIRE (secondLaunch.project().folder() == firstFolder);

        duet::gui::ViewState restored;
        restored.readFrom (secondLaunch.project().viewState());
        REQUIRE (restored.browserWidthPx() == 277);
    }

    std::filesystem::remove_all (firstFolder);
    ProjectLifecycle afterFolderWasRemoved { settings, projects };
    REQUIRE (afterFolderWasRemoved.launch());
    REQUIRE (afterFolderWasRemoved.project().folder() == firstFolder);
    REQUIRE (afterFolderWasRemoved.lastError().empty());
}

TEST_CASE ("changing the projects directory moves only later untitled projects")
{
    const TempProject temp;
    StoredSettings settings;
    const auto firstProjectsDirectory = temp.folder() / "First";
    const auto laterProjectsDirectory = temp.folder() / "Later";
    ProjectLifecycle lifecycle { settings, firstProjectsDirectory };
    REQUIRE (lifecycle.launch());
    const auto firstProject = lifecycle.project().folder();

    duet::gui::setProjectsDirectory (settings, laterProjectsDirectory);

    REQUIRE (lifecycle.createNew (UnsavedDecision::discard));
    REQUIRE (lifecycle.project().folder() == laterProjectsDirectory / "Untitled 1");
    REQUIRE (std::filesystem::is_regular_file (duet::persistence::editFile (firstProject)));
}

TEST_CASE ("close decisions cancel, discard, or save a dirty project")
{
    const TempProject temp;
    StoredSettings settings;
    const auto projects = temp.folder() / "Projects";
    std::filesystem::path discardedFolder;

    {
        ProjectLifecycle lifecycle { settings, projects };
        REQUIRE (lifecycle.launch());
        discardedFolder = lifecycle.project().folder();
        lifecycle.project().session().performAction (
            "Add bass", [] (auto& ops) { ops.createTrack (TrackKind::audio, "Bass"); });

        REQUIRE_FALSE (lifecycle.mayClose (UnsavedDecision::cancel));
        REQUIRE (lifecycle.project().hasUnsavedChanges());
        REQUIRE (lifecycle.mayClose (UnsavedDecision::discard));
    }

    auto discarded = duet::persistence::Project::open (discardedFolder);
    REQUIRE (discarded != nullptr);
    REQUIRE (discarded->session().tracks().size() == 2);
    discarded.reset();

    ProjectLifecycle lifecycle { settings, projects };
    REQUIRE (lifecycle.launch());
    lifecycle.project().session().performAction (
        "Add keys", [] (auto& ops) { ops.createTrack (TrackKind::audio, "Keys"); });
    REQUIRE (lifecycle.mayClose (UnsavedDecision::save));
    REQUIRE_FALSE (lifecycle.project().hasUnsavedChanges());

    auto saved = duet::persistence::Project::open (lifecycle.project().folder());
    REQUIRE (saved != nullptr);
    REQUIRE (saved->session().tracks().back().name == "Keys");
}

TEST_CASE ("New and Open leave a dirty project alone when the prompt is cancelled")
{
    const TempProject temp;
    StoredSettings settings;
    const auto projects = temp.folder() / "Projects";
    const auto otherFolder = temp.folder() / "Other";
    auto other = duet::persistence::Project::create (otherFolder);
    REQUIRE (other != nullptr);
    other.reset();

    ProjectLifecycle lifecycle { settings, projects };
    REQUIRE (lifecycle.launch());
    const auto currentFolder = lifecycle.project().folder();
    lifecycle.project().session().performAction (
        "Add bass", [] (auto& ops) { ops.createTrack (TrackKind::audio, "Bass"); });

    REQUIRE_FALSE (lifecycle.createNew (UnsavedDecision::cancel));
    REQUIRE (lifecycle.project().folder() == currentFolder);
    REQUIRE (lifecycle.project().hasUnsavedChanges());

    REQUIRE_FALSE (lifecycle.open (
        otherFolder, duet::persistence::RecoveryChoice::decline, UnsavedDecision::cancel));
    REQUIRE (lifecycle.project().folder() == currentFolder);
    REQUIRE (lifecycle.project().hasUnsavedChanges());
}

TEST_CASE ("Save As changes the open project name and later saves stay in the copy")
{
    const TempProject temp;
    StoredSettings settings;
    ProjectLifecycle lifecycle { settings, temp.folder() / "Projects" };
    REQUIRE (lifecycle.launch());
    const auto sourceFolder = lifecycle.project().folder();
    const auto destinationFolder = temp.folder() / "Album" / "Nocturne";
    std::filesystem::create_directories (destinationFolder);

    lifecycle.project().session().performAction (
        "Add bass", [] (auto& ops) { ops.createTrack (TrackKind::audio, "Bass"); });
    REQUIRE (lifecycle.saveAs (destinationFolder));
    REQUIRE (lifecycle.projectName() == "Nocturne");
    REQUIRE (lifecycle.project().folder() == destinationFolder);
    REQUIRE (std::filesystem::is_regular_file (duet::persistence::editFile (sourceFolder)));

    lifecycle.project().session().performAction (
        "Rename bass",
        [&lifecycle] (auto& ops)
        { ops.renameTrack (lifecycle.project().session().tracks().back().track, "Low End"); });
    REQUIRE (lifecycle.save());

    const auto reopened = duet::persistence::Project::open (destinationFolder);
    REQUIRE (reopened != nullptr);
    REQUIRE (reopened->session().tracks().back().name == "Low End");
}

TEST_CASE ("the app-global autosave choice is applied to each project that opens")
{
    const TempProject temp;
    StoredSettings settings;
    duet::gui::setAutosaveInterval (settings, duet::persistence::AutosaveInterval::twoMinutes);
    ProjectLifecycle lifecycle { settings, temp.folder() / "Projects" };

    REQUIRE (lifecycle.launch());
    REQUIRE (lifecycle.project().autosaveInterval()
             == duet::persistence::AutosaveInterval::twoMinutes);

    duet::gui::setAutosaveInterval (settings, duet::persistence::AutosaveInterval::off);
    REQUIRE (lifecycle.createNew (UnsavedDecision::discard));
    REQUIRE (lifecycle.project().autosaveInterval() == duet::persistence::AutosaveInterval::off);
}

TEST_CASE ("recent projects are newest first and missing folders are dropped")
{
    const TempProject temp;
    StoredSettings settings;
    const auto projects = temp.folder() / "Projects";
    std::filesystem::path firstFolder;
    std::filesystem::path secondFolder;

    {
        ProjectLifecycle lifecycle { settings, projects };
        REQUIRE (lifecycle.launch());
        firstFolder = lifecycle.project().folder();
        REQUIRE (lifecycle.createNew (UnsavedDecision::discard));
        secondFolder = lifecycle.project().folder();
        REQUIRE (lifecycle.recentProjects()
                 == std::vector<std::filesystem::path> { secondFolder, firstFolder });
    }

    std::filesystem::remove_all (firstFolder);
    ProjectLifecycle nextLaunch { settings, projects };
    REQUIRE (nextLaunch.launch());
    REQUIRE (nextLaunch.recentProjects() == std::vector<std::filesystem::path> { secondFolder });
}
