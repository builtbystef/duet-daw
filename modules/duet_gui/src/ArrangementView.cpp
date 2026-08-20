#include <duet/gui/ArrangementView.h>

#include <duet/gui/TimelineClock.h>
#include <duet/gui/ViewState.h>

#include <algorithm>
#include <cmath>

namespace duet::gui
{
namespace
{
    /** What a wheel turned that far means to a zoom. */
    double zoomFactorFor (double notches)
    {
        return std::pow (ArrangementView::zoomFactorPerNotch, notches);
    }

    /** What a wheel turned that far means to a scroll, in pixels. Away from the
        producer moves the view towards the start of the project, which is up on
        the screen and left on the timeline.
    */
    int scrollPixelsFor (double notches)
    {
        return static_cast<int> (std::lround (-notches * ArrangementView::scrollPixelsPerNotch));
    }
} // namespace

ArrangementView::ArrangementView (ViewState& projectView) : view (projectView) {}

//==============================================================================
void ArrangementView::setClock (TimelineClock* projectClock)
{
    clock = projectClock;
    refresh();
}

void ArrangementView::refresh()
{
    if (clock != nullptr)
        timeline.setBeatsPerBar (clock->beatsPerBar());
}

void ArrangementView::setWidthPx (int widthPx) { timeline.setWidthPx (widthPx); }

//==============================================================================
void ArrangementView::scroll (const ScrollGesture& gesture)
{
    if (gesture.ctrl)
    {
        // Ctrl zooms, and Shift is what says which way: the timeline about the
        // pointer, or the tracks' own heights.
        if (gesture.shift)
            view.scaleTrackHeights (zoomFactorFor (gesture.deltaY));
        else
            timeline.zoomAt (gesture.pointerX, zoomFactorFor (gesture.deltaY));

        return;
    }

    // A trackpad reports sideways travel of its own, and Shift is how a wheel
    // that has none says the same thing.
    const auto sideways = gesture.deltaX + (gesture.shift ? gesture.deltaY : 0.0);

    timeline.scrollByPixels (scrollPixelsFor (sideways));

    if (! gesture.shift)
        view.setVScrollPx (view.vScrollPx() + scrollPixelsFor (gesture.deltaY));
}

void ArrangementView::perform (Command command)
{
    switch (command)
    {
        case Command::zoomIn:
            timeline.zoomAtCentre (zoomFactorPerKeyPress);
            break;

        case Command::zoomOut:
            timeline.zoomAtCentre (1.0 / zoomFactorPerKeyPress);
            break;

        case Command::zoomToFit:
            timeline.fitToWidth (clock != nullptr ? clock->contentLengthBeats() : 0.0);
            break;

        default:
            // Every other command is another surface's.
            break;
    }
}

//==============================================================================
int ArrangementView::playheadX() const
{
    return timeline.beatsToX (clock != nullptr ? clock->playheadBeats() : 0.0);
}

bool ArrangementView::isPlaying() const { return clock != nullptr && clock->isPlaying(); }

void ArrangementView::clickRuler (int px)
{
    if (clock != nullptr)
        clock->setPlayheadBeats (std::max (0.0, timeline.xToBeats (px)));
}
} // namespace duet::gui
