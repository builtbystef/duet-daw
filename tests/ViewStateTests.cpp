#include <duet/gui/ViewState.h>

#include <duet/persistence/DataNode.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using duet::gui::BottomTab;
using duet::gui::ViewState;

namespace
{
/** Two refs that name tracks. What they are worth does not matter here — only
    that a view keeps them apart.
*/
constexpr duet::model::TrackRef bass = 41;
constexpr duet::model::TrackRef keys = 77;
} // namespace

TEST_CASE ("a dock reopens at the width it was collapsed at")
{
    ViewState view;

    view.setBrowserWidthPx (260);
    view.setBrowserVisible (false);

    REQUIRE_FALSE (view.browserVisible());

    view.setBrowserVisible (true);

    REQUIRE (view.browserVisible());
    REQUIRE (view.browserWidthPx() == 260);
}

TEST_CASE ("a divider drag stops at the sizes a dock is still usable at")
{
    ViewState view;

    SECTION ("the browser")
    {
        view.setBrowserWidthPx (0);
        REQUIRE (view.browserWidthPx() == ViewState::minimumBrowserWidthPx);

        view.setBrowserWidthPx (100000);
        REQUIRE (view.browserWidthPx() == ViewState::maximumBrowserWidthPx);
    }

    SECTION ("the Collaborator")
    {
        view.setCollaboratorWidthPx (0);
        REQUIRE (view.collaboratorWidthPx() == ViewState::minimumCollaboratorWidthPx);

        view.setCollaboratorWidthPx (100000);
        REQUIRE (view.collaboratorWidthPx() == ViewState::maximumCollaboratorWidthPx);
    }

    SECTION ("the bottom panel")
    {
        view.setBottomHeightPx (0);
        REQUIRE (view.bottomHeightPx() == ViewState::minimumBottomHeightPx);

        view.setBottomHeightPx (100000);
        REQUIRE (view.bottomHeightPx() == ViewState::maximumBottomHeightPx);
    }
}

TEST_CASE ("a track the view has never been told about has the default row")
{
    const ViewState view;

    REQUIRE (view.trackHeightPx (bass) == ViewState::defaultTrackHeightPx);
    REQUIRE_FALSE (view.lanesExpanded (bass));
}

TEST_CASE ("one track's row is not another's")
{
    ViewState view;

    view.setTrackHeightPx (bass, 120);
    view.setLanesExpanded (bass, true);

    REQUIRE (view.trackHeightPx (bass) == 120);
    REQUIRE (view.lanesExpanded (bass));
    REQUIRE (view.trackHeightPx (keys) == ViewState::defaultTrackHeightPx);
    REQUIRE_FALSE (view.lanesExpanded (keys));
}

TEST_CASE ("a vertical zoom is every track row growing at once")
{
    ViewState view;

    view.setTrackHeightPx (bass, 120);
    view.setTrackHeightPx (keys, 60);
    view.scaleTrackHeights (2.0);

    REQUIRE (view.trackHeightPx (bass) == 240);
    REQUIRE (view.trackHeightPx (keys) == 120);

    // A row stops where a track is still a track: the tallest row is as far as
    // a vertical zoom goes, and the shortest is as far down as it comes.
    view.scaleTrackHeights (100.0);
    REQUIRE (view.trackHeightPx (bass) == ViewState::maximumTrackHeightPx);

    view.scaleTrackHeights (0.0001);
    REQUIRE (view.trackHeightPx (bass) == ViewState::minimumTrackHeightPx);
}

TEST_CASE ("a view comes back from the VIEW tree exactly as it went in")
{
    ViewState view;

    view.setHZoomPxPerBeat (48.5);
    view.setHScrollBeats (12.25);
    view.setVScrollPx (140);
    view.setGridSize (duet::gui::GridSize::eighth);
    view.setFollowPlayhead (false);
    view.setBrowserWidthPx (260);
    view.setCollaboratorVisible (false);
    view.setBottomHeightPx (320);
    view.setBottomTab (BottomTab::mixer);
    view.setTrackHeightPx (bass, 120);
    view.setLanesExpanded (bass, true);
    view.setTrackHeightPx (keys, 96);

    const auto stored = view.toData();

    REQUIRE (stored.type() == "VIEW");

    ViewState reopened;
    reopened.readFrom (stored);

    REQUIRE_THAT (reopened.hZoomPxPerBeat(), WithinAbs (48.5, 1e-9));
    REQUIRE_THAT (reopened.hScrollBeats(), WithinAbs (12.25, 1e-9));
    REQUIRE (reopened.vScrollPx() == 140);
    REQUIRE (reopened.gridSize() == duet::gui::GridSize::eighth);
    REQUIRE_FALSE (reopened.followPlayhead());
    REQUIRE (reopened.browserVisible());
    REQUIRE (reopened.browserWidthPx() == 260);
    REQUIRE_FALSE (reopened.collaboratorVisible());
    REQUIRE (reopened.bottomVisible());
    REQUIRE (reopened.bottomHeightPx() == 320);
    REQUIRE (reopened.bottomTab() == BottomTab::mixer);
    REQUIRE (reopened.trackHeightPx (bass) == 120);
    REQUIRE (reopened.lanesExpanded (bass));
    REQUIRE (reopened.trackHeightPx (keys) == 96);
    REQUIRE_FALSE (reopened.lanesExpanded (keys));
}

TEST_CASE ("a project saved before the view existed opens on the defaults")
{
    const duet::persistence::DataNode empty { "VIEW" };

    ViewState view;
    view.readFrom (empty);

    REQUIRE (view.browserVisible());
    REQUIRE (view.collaboratorVisible());
    REQUIRE (view.bottomVisible());
    REQUIRE (view.bottomTab() == BottomTab::pianoRoll);
    REQUIRE (view.browserWidthPx() == ViewState::defaultBrowserWidthPx);
}

TEST_CASE ("a VIEW tree written by hand cannot hand the interface a size it cannot lay out")
{
    duet::persistence::DataNode broken { "VIEW" };
    broken.set ("browserWidthPx", 3);
    broken.set ("hZoomPxPerBeat", 0.0);

    ViewState view;
    view.readFrom (broken);

    REQUIRE (view.browserWidthPx() == ViewState::minimumBrowserWidthPx);
    REQUIRE (view.hZoomPxPerBeat() >= ViewState::minimumZoomPxPerBeat);
}
