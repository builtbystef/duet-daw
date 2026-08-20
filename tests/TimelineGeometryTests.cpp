#include <duet/gui/TimelineGeometry.h>

#include <duet/gui/ViewState.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using duet::gui::GridWeight;
using duet::gui::TimelineGeometry;
using duet::gui::ViewState;

TEST_CASE ("a beat is where the zoom and the scroll say it is")
{
    // The worked example spec 535bbo names: 40 px/beat, scrolled to beat 8.0.
    ViewState view;
    const TimelineGeometry geometry { view };

    view.setHZoomPxPerBeat (40.0);
    view.setHScrollBeats (8.0);

    REQUIRE (geometry.beatsToX (10.0) == 80);
    REQUIRE_THAT (geometry.xToBeats (80), WithinAbs (10.0, 1e-9));

    // The left edge is the beat the view is scrolled to, and beats before it are
    // off the left of the canvas.
    REQUIRE (geometry.beatsToX (8.0) == 0);
    REQUIRE (geometry.beatsToX (7.5) == -20);
    REQUIRE_THAT (geometry.xToBeats (0), WithinAbs (8.0, 1e-9));
}

TEST_CASE ("the grid is the finest subdivision the zoom has room for")
{
    ViewState view;
    TimelineGeometry geometry { view };

    SECTION ("the worked examples spec 535bbo names, in 4/4")
    {
        view.setHZoomPxPerBeat (20.0);

        // Beat lines are 20 px apart and quarter-beat lines would be 5.
        REQUIRE_THAT (geometry.gridFor().subdivisionBeats, WithinAbs (1.0, 1e-9));

        view.setHZoomPxPerBeat (80.0);

        // Quarter-beats — 1/16 notes — are 20 px apart.
        REQUIRE_THAT (geometry.gridFor().subdivisionBeats, WithinAbs (0.25, 1e-9));
    }

    SECTION ("18 px is the line either side of the boundary")
    {
        view.setHZoomPxPerBeat (72.0);
        REQUIRE_THAT (geometry.gridFor().subdivisionBeats, WithinAbs (0.25, 1e-9));

        view.setHZoomPxPerBeat (71.0);
        REQUIRE_THAT (geometry.gridFor().subdivisionBeats, WithinAbs (0.5, 1e-9));
    }

    SECTION ("a zoom with no room for a bar counts bars in twos")
    {
        view.setHZoomPxPerBeat (4.0);

        // A bar is 16 px, so the grid is the two bars that are 32.
        REQUIRE_THAT (geometry.gridFor().subdivisionBeats, WithinAbs (8.0, 1e-9));
    }

    SECTION ("the bar the grid is counted in is the project's own")
    {
        geometry.setBeatsPerBar (3.0);
        view.setHZoomPxPerBeat (10.0);

        const auto grid = geometry.gridFor();

        REQUIRE_THAT (grid.barBeats, WithinAbs (3.0, 1e-9));
        REQUIRE_THAT (grid.subdivisionBeats, WithinAbs (3.0, 1e-9));
    }
}

TEST_CASE ("zooming at a pointer leaves the beat under the pointer where it is")
{
    // The worked example spec 535bbo names: beat 8.0 under x = 400.
    ViewState view;
    TimelineGeometry geometry { view };

    view.setHZoomPxPerBeat (100.0);
    view.setHScrollBeats (4.0);

    REQUIRE (geometry.beatsToX (8.0) == 400);

    SECTION ("zooming in")
    {
        geometry.zoomAt (400, 2.0);

        REQUIRE_THAT (view.hZoomPxPerBeat(), WithinAbs (200.0, 1e-9));
        REQUIRE (geometry.beatsToX (8.0) == 400);
    }

    SECTION ("zooming out")
    {
        geometry.zoomAt (400, 0.5);

        REQUIRE_THAT (view.hZoomPxPerBeat(), WithinAbs (50.0, 1e-9));
        REQUIRE (geometry.beatsToX (8.0) == 400);
    }
}

TEST_CASE ("a bar line a beat line and a fine line are three different lines")
{
    ViewState view;
    TimelineGeometry geometry { view };

    geometry.setWidthPx (200);
    view.setHZoomPxPerBeat (80.0);

    const auto lines = geometry.gridLines();

    REQUIRE (lines.size() == 11);
    REQUIRE (lines.front().x == 0);
    REQUIRE (lines.back().x == 200);

    // Beat 0 opens a bar, the quarter-beats between the beats are fine lines,
    // and beat 1 is a beat.
    REQUIRE (lines[0].weight == GridWeight::bar);
    REQUIRE (lines[1].weight == GridWeight::fine);
    REQUIRE (lines[2].weight == GridWeight::fine);
    REQUIRE (lines[3].weight == GridWeight::fine);
    REQUIRE (lines[4].weight == GridWeight::beat);
    REQUIRE_THAT (lines[4].beats, WithinAbs (1.0, 1e-9));
}

TEST_CASE ("the ruler labels bars and beats")
{
    ViewState view;
    TimelineGeometry geometry { view };

    geometry.setWidthPx (400);

    SECTION ("a zoom with room for bars only")
    {
        view.setHZoomPxPerBeat (20.0);

        const auto labels = geometry.rulerLabels();

        REQUIRE (labels.size() == 6);
        REQUIRE (labels[0].text == "1");
        REQUIRE (labels[1].text == "2");

        // The worked example spec 535bbo names: in 4/4, bar 3 is at beat 8.0.
        REQUIRE (labels[2].text == "3");
        REQUIRE_THAT (labels[2].beats, WithinAbs (8.0, 1e-9));
        REQUIRE (labels[2].x == 160);
    }

    SECTION ("a zoom with room for the beats inside a bar")
    {
        view.setHZoomPxPerBeat (80.0);

        const auto labels = geometry.rulerLabels();

        REQUIRE (labels[0].text == "1");
        REQUIRE (labels[1].text == "1.2");
        REQUIRE (labels[3].text == "1.4");

        // A bar keeps its own number: the producer reads bar 2, not bar 2 beat 1.
        REQUIRE (labels[4].text == "2");
    }
}

TEST_CASE ("the timeline stops at the start of the project")
{
    ViewState view;
    TimelineGeometry geometry { view };

    view.setHZoomPxPerBeat (40.0);
    view.setHScrollBeats (2.0);
    geometry.scrollByPixels (-400);

    REQUIRE_THAT (view.hScrollBeats(), WithinAbs (0.0, 1e-9));
    REQUIRE (geometry.beatsToX (0.0) == 0);

    geometry.scrollByPixels (80);
    REQUIRE_THAT (view.hScrollBeats(), WithinAbs (2.0, 1e-9));
}

TEST_CASE ("the ends of the zoom range still have a grid")
{
    ViewState view;
    TimelineGeometry geometry { view };

    SECTION ("zoomed all the way in, the finest grid is the one in force")
    {
        view.setHZoomPxPerBeat (ViewState::maximumZoomPxPerBeat);

        REQUIRE_THAT (geometry.gridFor().subdivisionBeats,
                      WithinAbs (TimelineGeometry::finestSubdivisionBeats, 1e-9));
    }

    SECTION ("zoomed all the way out, the coarsest grid still has room to draw")
    {
        view.setHZoomPxPerBeat (ViewState::minimumZoomPxPerBeat);

        const auto grid = geometry.gridFor();

        REQUIRE (grid.subdivisionBeats
                 <= TimelineGeometry::coarsestSubdivisionBars * grid.barBeats);
        REQUIRE (grid.subdivisionBeats * ViewState::minimumZoomPxPerBeat
                 >= TimelineGeometry::minimumGridSpacingPx);
    }

    SECTION ("a zoom gesture past the end of the range stops at the end")
    {
        view.setHZoomPxPerBeat (ViewState::maximumZoomPxPerBeat);
        geometry.zoomAt (100, 4.0);

        REQUIRE_THAT (view.hZoomPxPerBeat(), WithinAbs (ViewState::maximumZoomPxPerBeat, 1e-9));
    }
}

TEST_CASE ("zoom to fit puts the whole project on the canvas")
{
    ViewState view;
    TimelineGeometry geometry { view };

    geometry.setWidthPx (400);
    view.setHScrollBeats (12.0);
    geometry.fitToWidth (16.0);

    REQUIRE_THAT (view.hZoomPxPerBeat(), WithinAbs (25.0, 1e-9));
    REQUIRE_THAT (view.hScrollBeats(), WithinAbs (0.0, 1e-9));
    REQUIRE (geometry.beatsToX (16.0) == 400);

    SECTION ("a project with nothing in it is left at a zoom that can be worked at")
    {
        geometry.fitToWidth (0.0);

        REQUIRE_THAT (view.hZoomPxPerBeat(), WithinAbs (ViewState::defaultZoomPxPerBeat, 1e-9));
    }
}

TEST_CASE ("zooming with a key is zooming about the middle of the canvas")
{
    ViewState view;
    TimelineGeometry geometry { view };

    geometry.setWidthPx (400);
    view.setHZoomPxPerBeat (100.0);
    view.setHScrollBeats (4.0);

    const auto middle = geometry.xToBeats (200);
    geometry.zoomAtCentre (2.0);

    REQUIRE_THAT (view.hZoomPxPerBeat(), WithinAbs (200.0, 1e-9));
    REQUIRE_THAT (geometry.xToBeats (200), WithinAbs (middle, 1e-9));
}
