#include <duet/gui/ViewState.h>

#include <duet/persistence/Project.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>

using duet::gui::BottomTab;
using duet::gui::ViewState;
using duet::model::TrackKind;
using duet::persistence::Project;
using duet::testing::TempProject;

namespace
{
/** The layout the producer left the project in: the worked example spec 535bbo
    and issue fcsez4 name.
*/
void leaveTheLayoutTheProducerSet (ViewState& view)
{
    view.setBrowserWidthPx (260);
    view.setCollaboratorVisible (false);
    view.setBottomHeightPx (320);
    view.setBottomTab (BottomTab::mixer);
}

/** What the shell does with a project: the view is asked for as a save begins,
    and nowhere else.
*/
void attach (Project& project, const ViewState& view)
{
    project.onCaptureViewState ([&view] { return view.toData(); });
}
} // namespace

TEST_CASE ("a project reopens on the layout the producer left it in")
{
    const TempProject temp;
    const auto folder = temp.folder() / "Nocturne";

    {
        const auto project = Project::create (folder);
        REQUIRE (project != nullptr);

        ViewState view;
        attach (*project, view);
        leaveTheLayoutTheProducerSet (view);

        REQUIRE (project->save());
    }

    const auto reopened = Project::open (folder);
    REQUIRE (reopened != nullptr);

    ViewState restored;
    restored.readFrom (reopened->viewState());

    REQUIRE (restored.browserWidthPx() == 260);
    REQUIRE_FALSE (restored.collaboratorVisible());
    REQUIRE (restored.bottomHeightPx() == 320);
    REQUIRE (restored.bottomTab() == BottomTab::mixer);

    // The layout came back, and nothing about coming back is a change: a
    // producer who reopens a project and closes it again is asked nothing.
    REQUIRE_FALSE (reopened->hasUnsavedChanges());
}

TEST_CASE ("moving a divider is not an edit, so it never makes a project dirty")
{
    const TempProject temp;
    const auto project = Project::create (temp.folder() / "Nocturne");
    REQUIRE (project != nullptr);

    ViewState view;
    attach (*project, view);

    leaveTheLayoutTheProducerSet (view);
    view.setBrowserVisible (false);
    view.setHScrollBeats (48.0);

    REQUIRE_FALSE (project->hasUnsavedChanges());

    project->session().performAction (
        "Add a bass track", [] (auto& ops) { ops.createTrack (TrackKind::audio, "Bass"); });

    REQUIRE (project->hasUnsavedChanges());
}

TEST_CASE ("a VIEW write never reaches the undo stack")
{
    const TempProject temp;
    const auto project = Project::create (temp.folder() / "Nocturne");
    REQUIRE (project != nullptr);

    ViewState view;
    attach (*project, view);

    project->session().performAction (
        "Add a bass track", [] (auto& ops) { ops.createTrack (TrackKind::audio, "Bass"); });

    const auto tracksAfterTheAction = project->session().tracks().size();

    // Everything the producer can do to the layout, and then the save that is
    // the only moment any of it is written.
    view.setBrowserVisible (false);
    view.setBrowserWidthPx (260);
    view.setBottomTab (BottomTab::mixer);
    REQUIRE (project->save());

    REQUIRE (project->session().undoNames().front() == "Add a bass track");
    REQUIRE (project->session().undo());

    // The Action is what undid, and the layout is where the producer left it.
    REQUIRE (project->session().tracks().size() == tracksAfterTheAction - 1);
    REQUIRE_FALSE (view.browserVisible());
    REQUIRE (view.browserWidthPx() == 260);
    REQUIRE (view.bottomTab() == BottomTab::mixer);
}

TEST_CASE ("a project nobody has given a view to opens on the interface's defaults")
{
    const TempProject temp;
    const auto folder = temp.folder() / "Nocturne";

    {
        const auto project = Project::create (folder);
        REQUIRE (project != nullptr);
        REQUIRE (project->save());
    }

    const auto reopened = Project::open (folder);
    REQUIRE (reopened != nullptr);

    ViewState restored;
    restored.readFrom (reopened->viewState());

    REQUIRE (restored.browserWidthPx() == ViewState::defaultBrowserWidthPx);
    REQUIRE (restored.collaboratorVisible());
}
