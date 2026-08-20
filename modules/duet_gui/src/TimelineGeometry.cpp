#include <duet/gui/TimelineGeometry.h>

#include <duet/gui/ViewState.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace duet::gui
{
namespace
{
    /** The grid's rungs below a beat, finest first: a 1/64, a 1/32, a 1/16 and
        an 1/8 note, written in the beats the timeline counts in.
    */
    constexpr std::array<double, 4> fineRungs { 1.0 / 16.0, 1.0 / 8.0, 1.0 / 4.0, 1.0 / 2.0 };

    /** True when a position in beats is a multiple of a spacing — which is what
        makes a line a bar line rather than a beat line. Written as a comparison
        with a tolerance because a time signature need not divide a beat evenly.
    */
    bool isMultipleOf (double beats, double spacing)
    {
        const auto steps = beats / spacing;

        return std::abs (steps - std::round (steps)) < 1e-6;
    }

    /** What a line at that position is: the start of a bar, a beat, or one of
        the subdivisions between them.
    */
    GridWeight weightFor (double beats, double barBeats)
    {
        if (isMultipleOf (beats, barBeats))
            return GridWeight::bar;

        return isMultipleOf (beats, 1.0) ? GridWeight::beat : GridWeight::fine;
    }

    /** The first multiple of a spacing at or after a position, as a count of
        steps.
    */
    long long firstStepAtOrAfter (double beats, double spacing)
    {
        return static_cast<long long> (std::ceil (beats / spacing - 1e-9));
    }

    /** A line's label: the bar it opens, or the beat inside that bar. */
    std::string labelFor (double beats, double barBeats)
    {
        const auto bar = static_cast<long long> (std::floor (beats / barBeats + 1e-9)) + 1;

        if (isMultipleOf (beats, barBeats))
            return std::to_string (bar);

        const auto intoTheBar = beats - (static_cast<double> (bar - 1) * barBeats);
        const auto beatInBar = static_cast<long long> (std::floor (intoTheBar + 1e-9)) + 1;

        return std::to_string (bar) + "." + std::to_string (beatInBar);
    }
} // namespace

TimelineGeometry::TimelineGeometry (ViewState& projectView) : view (projectView) {}

//==============================================================================
double TimelineGeometry::xToBeats (int px) const
{
    return view.hScrollBeats() + static_cast<double> (px) / view.hZoomPxPerBeat();
}

int TimelineGeometry::beatsToX (double beats) const
{
    return static_cast<int> (std::lround ((beats - view.hScrollBeats()) * view.hZoomPxPerBeat()));
}

//==============================================================================
void TimelineGeometry::zoomAt (int anchorPx, double factor)
{
    const auto anchored = xToBeats (anchorPx);

    view.setHZoomPxPerBeat (view.hZoomPxPerBeat() * factor);

    // Read back rather than multiply: the zoom the view took may be the end of
    // its range, and the beat has to stay under a pointer that could not zoom
    // as far as it was asked to.
    view.setHScrollBeats (anchored - static_cast<double> (anchorPx) / view.hZoomPxPerBeat());
}

void TimelineGeometry::zoomAtCentre (double factor) { zoomAt (canvasWidth / 2, factor); }

void TimelineGeometry::fitToWidth (double contentLengthBeats)
{
    // A project with nothing in it fits into any width at all, so fitting it
    // would ask for the widest zoom there is. The zoom a new project opens at
    // is the honest answer instead.
    view.setHZoomPxPerBeat (contentLengthBeats > 0.0 && canvasWidth > 0
                                ? static_cast<double> (canvasWidth) / contentLengthBeats
                                : ViewState::defaultZoomPxPerBeat);

    view.setHScrollBeats (0.0);
}

void TimelineGeometry::scrollByPixels (int px)
{
    view.setHScrollBeats (view.hScrollBeats() + static_cast<double> (px) / view.hZoomPxPerBeat());
}

//==============================================================================
void TimelineGeometry::setBeatsPerBar (double beats) { barBeats = std::max (1.0, beats); }

void TimelineGeometry::setWidthPx (int newWidth) { canvasWidth = std::max (0, newWidth); }

GridSpec TimelineGeometry::gridFor() const
{
    return { subdivisionFor (minimumGridSpacingPx, false), barBeats };
}

double TimelineGeometry::subdivisionFor (double minimumSpacingPx, bool neverFinerThanABeat) const
{
    const auto zoom = view.hZoomPxPerBeat();

    // The comparison carries a tolerance because the boundary is a
    // multiplication: a quarter beat at 72 px/beat is exactly 18 px, and the
    // grid it names is the one that has room.
    const auto hasRoom = [zoom, minimumSpacingPx] (double subdivision)
    { return subdivision * zoom >= minimumSpacingPx - 1e-9; };

    // Finest first, so that the first rung with room is the finest one with
    // room. Below a beat the rungs halve a note value; above it they count
    // bars, and count them in twos once a single bar has stopped fitting.
    if (! neverFinerThanABeat)
        for (const auto subdivision : fineRungs)
            if (hasRoom (subdivision))
                return subdivision;

    if (hasRoom (1.0))
        return 1.0;

    for (int doublings = 0;; ++doublings)
    {
        const auto bars = std::pow (2.0, doublings);

        if (bars >= coarsestSubdivisionBars || hasRoom (bars * barBeats))
            return bars * barBeats;
    }
}

std::vector<GridLine> TimelineGeometry::gridLines() const
{
    const auto grid = gridFor();
    std::vector<GridLine> lines;

    for (auto step = firstStepAtOrAfter (view.hScrollBeats(), grid.subdivisionBeats);; ++step)
    {
        const auto beats = static_cast<double> (step) * grid.subdivisionBeats;
        const auto x = beatsToX (beats);

        if (x > canvasWidth)
            return lines;

        lines.push_back ({ x, beats, weightFor (beats, grid.barBeats) });
    }
}

std::vector<RulerLabel> TimelineGeometry::rulerLabels() const
{
    const auto spacing = subdivisionFor (minimumLabelSpacingPx, true);
    std::vector<RulerLabel> labels;

    for (auto step = firstStepAtOrAfter (view.hScrollBeats(), spacing);; ++step)
    {
        const auto beats = static_cast<double> (step) * spacing;
        const auto x = beatsToX (beats);

        if (x > canvasWidth)
            return labels;

        labels.push_back ({ x, beats, labelFor (beats, barBeats) });
    }
}
} // namespace duet::gui
