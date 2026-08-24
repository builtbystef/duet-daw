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

void ArrangementView::setSession (duet::model::Session* openProject)
{
    session = openProject;
    refresh();
}

void ArrangementView::refresh()
{
    if (clock != nullptr)
        timeline.setBeatsPerBar (clock->beatsPerBar());

    if (session != nullptr)
    {
        std::vector<duet::model::TrackRef> refs;

        for (const auto& track : session->tracks())
            refs.push_back (track.track);

        view.syncTracks (refs);
    }
}

void ArrangementView::setWidthPx (int widthPx) { timeline.setWidthPx (widthPx); }

void ArrangementView::setHeightPx (int newHeightPx)
{
    heightPx = std::max (0, newHeightPx);

    if (heightPx > 0)
        view.setVScrollPx (std::min (view.vScrollPx(), std::max (0, contentHeightPx() - heightPx)));
}

std::vector<TrackDrawing> ArrangementView::tracks()
{
    std::vector<TrackDrawing> drawings;

    if (session == nullptr)
        return drawings;

    const auto modelTracks = session->tracks();
    std::vector<duet::model::TrackRef> refs;
    refs.reserve (modelTracks.size());

    for (const auto& track : modelTracks)
        refs.push_back (track.track);

    view.syncTracks (refs);

    auto y = -view.vScrollPx();
    const auto beatsPerSecond = std::max (1.0, session->tempoBpm()) / 60.0;

    for (const auto& track : modelTracks)
    {
        TrackDrawing drawing;
        drawing.track = track.track;
        drawing.name = track.name;
        drawing.kind = track.kind;
        drawing.colour = track.colour;
        drawing.muted = track.muted;
        drawing.soloed = track.soloed;
        drawing.recordArmed = track.recordArmed;
        drawing.y = y;
        drawing.height = view.trackHeightPx (track.track);

        for (const auto& clip : track.clips)
        {
            ClipDrawing clipDrawing;
            clipDrawing.clip = clip.clip;
            clipDrawing.name = clip.name;
            clipDrawing.x = timeline.beatsToX (clip.startSeconds * beatsPerSecond);
            const auto right =
                timeline.beatsToX ((clip.startSeconds + clip.lengthSeconds) * beatsPerSecond);
            clipDrawing.width = std::max (1, right - clipDrawing.x);
            clipDrawing.holdsMidi = clip.holdsMidi;
            clipDrawing.sourceFile = clip.sourceFile;

            if (clip.holdsMidi)
                for (const auto& note : session->notes (clip.clip))
                    clipDrawing.notes.push_back (
                        { timeline.beatsToX (clip.startSeconds * beatsPerSecond + note.startBeats),
                          std::max (1,
                                    timeline.beatsToX (clip.startSeconds * beatsPerSecond
                                                       + note.startBeats + note.lengthBeats)
                                        - timeline.beatsToX (clip.startSeconds * beatsPerSecond
                                                             + note.startBeats)),
                          note.pitch });

            drawing.clips.push_back (std::move (clipDrawing));
        }

        y += drawing.height;
        drawings.push_back (std::move (drawing));
    }

    return drawings;
}

int ArrangementView::contentHeightPx() const
{
    if (session == nullptr)
        return addTrackRowHeightPx;

    auto contentHeight = addTrackRowHeightPx;

    for (const auto& track : session->tracks())
        contentHeight += view.trackHeightPx (track.track);

    return contentHeight;
}

int ArrangementView::addTrackRowY() const
{
    return contentHeightPx() - addTrackRowHeightPx - view.vScrollPx();
}

//==============================================================================
duet::model::TrackRef ArrangementView::addTrack (duet::model::TrackKind kind)
{
    if (session == nullptr)
        return duet::model::noTrack;

    duet::model::TrackRef added = duet::model::noTrack;
    const auto midi = kind == duet::model::TrackKind::midi;
    session->performAction (
        midi ? "Add MIDI Track" : "Add Audio Track",
        [&] (auto& ops) { added = ops.createTrack (kind, midi ? "MIDI Track" : "Audio Track"); });
    view.ensureTrack (added);
    return added;
}

void ArrangementView::reorderTrack (duet::model::TrackRef track, int newIndex)
{
    if (session == nullptr)
        return;

    const auto modelTracks = session->tracks();
    const auto current =
        std::find_if (modelTracks.begin(),
                      modelTracks.end(),
                      [track] (const auto& candidate) { return candidate.track == track; });
    if (current == modelTracks.end())
        return;

    const auto currentIndex = static_cast<int> (std::distance (modelTracks.begin(), current));
    const auto clampedIndex = std::clamp (newIndex, 0, static_cast<int> (modelTracks.size()) - 1);

    if (currentIndex == clampedIndex)
        return;

    session->performAction ("Reorder Track",
                            [&] (auto& ops) { ops.moveTrack (track, clampedIndex); });
}

void ArrangementView::renameTrack (duet::model::TrackRef track, std::string name)
{
    if (session != nullptr && ! name.empty() && session->track (track).name != name)
        session->performAction ("Rename Track", [&] (auto& ops) { ops.renameTrack (track, name); });
}

duet::model::TrackRef ArrangementView::duplicateTrack (duet::model::TrackRef track)
{
    if (session == nullptr)
        return duet::model::noTrack;

    duet::model::TrackRef copy = duet::model::noTrack;
    session->performAction ("Duplicate Track",
                            [&] (auto& ops) { copy = ops.duplicateTrack (track); });
    view.ensureTrack (copy);
    return copy;
}

void ArrangementView::deleteTrack (duet::model::TrackRef track)
{
    if (session == nullptr)
        return;

    session->performAction ("Delete Track", [&] (auto& ops) { ops.removeTrack (track); });
    view.removeTrack (track);
}

void ArrangementView::setTrackColour (duet::model::TrackRef track, duet::model::TrackColour colour)
{
    if (session != nullptr && session->track (track).colour != colour)
        session->performAction ("Set Track Colour",
                                [&] (auto& ops) { ops.setTrackColour (track, colour); });
}

void ArrangementView::toggleMute (duet::model::TrackRef track)
{
    if (session != nullptr)
    {
        const auto muted = session->track (track).muted;
        session->performAction ("Mute Track",
                                [&] (auto& ops) { ops.setTrackMute (track, ! muted); });
    }
}

void ArrangementView::toggleSolo (duet::model::TrackRef track)
{
    if (session != nullptr)
    {
        const auto soloed = session->track (track).soloed;
        session->performAction ("Solo Track",
                                [&] (auto& ops) { ops.setTrackSolo (track, ! soloed); });
    }
}

void ArrangementView::toggleRecordArm (duet::model::TrackRef track)
{
    if (session != nullptr)
        session->setTrackRecordArmed (track, ! session->track (track).recordArmed);
}

void ArrangementView::resizeTrack (duet::model::TrackRef track, int newHeightPx)
{
    view.setTrackHeightPx (track, newHeightPx);
    setHeightPx (heightPx);
}

//==============================================================================
void ArrangementView::scroll (const ScrollGesture& gesture)
{
    if (gesture.ctrl)
    {
        // Ctrl zooms, and Shift is what says which way: the timeline about the
        // pointer, or the tracks' own heights.
        if (gesture.shift)
        {
            view.scaleTrackHeights (zoomFactorFor (gesture.deltaY));
            setHeightPx (heightPx);
        }
        else
            timeline.zoomAt (gesture.pointerX, zoomFactorFor (gesture.deltaY));

        return;
    }

    // A trackpad reports sideways travel of its own, and Shift is how a wheel
    // that has none says the same thing.
    const auto sideways = gesture.deltaX + (gesture.shift ? gesture.deltaY : 0.0);

    timeline.scrollByPixels (scrollPixelsFor (sideways));

    if (! gesture.shift)
    {
        view.setVScrollPx (view.vScrollPx() + scrollPixelsFor (gesture.deltaY));
        setHeightPx (heightPx);
    }
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
