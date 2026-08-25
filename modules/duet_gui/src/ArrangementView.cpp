#include <duet/gui/ArrangementView.h>

#include <duet/gui/Snap.h>
#include <duet/gui/TimelineClock.h>
#include <duet/gui/ViewState.h>

#include <algorithm>
#include <cmath>
#include <limits>

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
        drawing.pan = track.pan;
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
            clipDrawing.colour = clip.colour;
            clipDrawing.sourceFile = clip.sourceFile;

            if (clip.holdsMidi)
                for (const auto& note : session->notes (clip.clip))
                    clipDrawing.notes.push_back (
                        { timeline.beatsToX ((clip.startSeconds - clip.contentOffsetSeconds)
                                                 * beatsPerSecond
                                             + note.startBeats),
                          std::max (
                              1,
                              timeline.beatsToX ((clip.startSeconds - clip.contentOffsetSeconds)
                                                     * beatsPerSecond
                                                 + note.startBeats + note.lengthBeats)
                                  - timeline.beatsToX (
                                      (clip.startSeconds - clip.contentOffsetSeconds)
                                          * beatsPerSecond
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

void ArrangementView::cyclePan (duet::model::TrackRef track)
{
    if (session == nullptr)
        return;
    const auto pan = session->track (track).pan;
    auto next = -1.0;
    if (pan < -0.5)
        next = 0.0;
    else if (pan < 0.5)
        next = 1.0;
    session->performAction ("Pan Track", [&] (auto& ops) { ops.setTrackPan (track, next); });
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
double ArrangementView::beatsPerSecond() const
{
    return session != nullptr ? std::max (1.0, session->tempoBpm()) / 60.0 : 2.0;
}

std::optional<duet::model::ClipInfo> ArrangementView::clipInfo (duet::model::ClipRef clip) const
{
    if (session != nullptr)
        for (const auto& track : session->tracks())
            for (const auto& candidate : track.clips)
                if (candidate.clip == clip)
                    return candidate;

    return {};
}

duet::model::TrackRef ArrangementView::trackOf (duet::model::ClipRef clip) const
{
    if (session != nullptr)
        for (const auto& track : session->tracks())
            for (const auto& candidate : track.clips)
                if (candidate.clip == clip)
                    return track.track;

    return duet::model::noTrack;
}

std::vector<SelectedItem> ArrangementView::allClipItems() const
{
    std::vector<SelectedItem> items;
    if (session != nullptr)
        for (const auto& track : session->tracks())
            for (const auto& clip : track.clips)
                items.push_back ({ SelectionKind::clip, clip.clip });
    return items;
}

void ArrangementView::rubberBand (SelectionRect rectangle, bool ctrlHeld)
{
    std::vector<SelectedItem> intersected;
    const auto right = rectangle.x + rectangle.width;
    const auto bottom = rectangle.y + rectangle.height;
    for (const auto& row : tracks())
        for (const auto& clip : row.clips)
            if (clip.x < right && clip.x + clip.width > rectangle.x && row.y < bottom
                && row.y + row.height > rectangle.y)
                intersected.push_back ({ SelectionKind::clip, clip.clip });

    currentSelection.rubberBand (intersected, ctrlHeld);
}

void ArrangementView::beginClipGesture (duet::model::ClipRef clip, ClipGestureKind kind)
{
    const auto info = clipInfo (clip);
    if (! info.has_value())
        return;

    gesture = Gesture { clip,           kind,  trackOf (clip),
                        trackOf (clip), *info, info->startSeconds * beatsPerSecond(),
                        false };
}

void ArrangementView::updateClipGesture (double destinationBeats,
                                         duet::model::TrackRef destinationTrack,
                                         bool altHeld,
                                         bool ctrlHeld)
{
    if (! gesture.has_value())
        return;

    gesture->destinationBeats = snapBeats (destinationBeats, timeline.gridFor(), altHeld);
    gesture->destinationTrack = destinationTrack;
    gesture->copy = ctrlHeld && gesture->kind == ClipGestureKind::move;
}

bool ArrangementView::completeClipGesture()
{
    if (session == nullptr || ! gesture.has_value())
        return false;

    const auto completed = *gesture;
    gesture.reset();
    const auto perSecond = beatsPerSecond();
    const auto startBeats = completed.original.startSeconds * perSecond;
    const auto lengthBeats = completed.original.lengthSeconds * perSecond;

    if (completed.destinationTrack == duet::model::noTrack)
        return false;

    switch (completed.kind)
    {
        case ClipGestureKind::move:
        {
            const auto seconds = std::max (0.0, completed.destinationBeats / perSecond);
            if (completed.copy)
            {
                duet::model::ClipRef copy = duet::model::noClip;
                session->performAction ("Copy Clip",
                                        [&] (auto& ops) {
                                            copy = ops.duplicateClip (completed.clip,
                                                                      completed.destinationTrack,
                                                                      seconds);
                                        });
                currentSelection.click (
                    { SelectionKind::clip, copy }, allClipItems(), false, false);
            }
            else
            {
                session->performAction (
                    "Move Clip",
                    [&] (auto& ops)
                    { ops.moveClip (completed.clip, completed.destinationTrack, seconds); });
                currentSelection.click (
                    { SelectionKind::clip, completed.clip }, allClipItems(), false, false);
            }
            return true;
        }

        case ClipGestureKind::trimLeft:
        {
            const auto endBeats = startBeats + lengthBeats;
            const auto newStart = std::clamp (
                completed.destinationBeats, 0.0, endBeats - timeline.gridFor().subdivisionBeats);
            session->performAction ("Trim Clip",
                                    [&] (auto& ops) {
                                        ops.trimClip (completed.clip,
                                                      newStart / perSecond,
                                                      (endBeats - newStart) / perSecond);
                                    });
            return true;
        }

        case ClipGestureKind::trimRight:
        {
            const auto newEnd = std::max (startBeats + timeline.gridFor().subdivisionBeats,
                                          completed.destinationBeats);
            session->performAction ("Trim Clip",
                                    [&] (auto& ops) {
                                        ops.trimClip (completed.clip,
                                                      startBeats / perSecond,
                                                      (newEnd - startBeats) / perSecond);
                                    });
            return true;
        }

        case ClipGestureKind::loop:
        {
            const auto contentBeats =
                completed.original.looped ? completed.original.loopLengthBeats : lengthBeats;
            if (contentBeats <= 0.0)
                return false;
            const auto repetitions = std::max (
                1.0, std::round ((completed.destinationBeats - startBeats) / contentBeats));
            session->performAction ("Loop Clip",
                                    [&] (auto& ops)
                                    {
                                        ops.setClipLoop (completed.clip, true, contentBeats);
                                        ops.trimClip (completed.clip,
                                                      repetitions * contentBeats / perSecond);
                                    });
            return true;
        }
    }

    return false;
}

void ArrangementView::cancelClipGesture() { gesture.reset(); }

bool ArrangementView::hasClipGesture() const { return gesture.has_value(); }

void ArrangementView::deleteSelected()
{
    if (session == nullptr)
        return;

    std::vector<duet::model::ClipRef> clips;
    for (const auto item : currentSelection.items())
        if (item.kind == SelectionKind::clip)
            clips.push_back (item.ref);

    if (clips.empty())
        return;

    session->performAction (clips.size() == 1 ? "Delete Clip" : "Delete Clips",
                            [&] (auto& ops)
                            {
                                for (const auto clip : clips)
                                    ops.deleteClip (clip);
                            });
    currentSelection.clear();
}

void ArrangementView::copySelected()
{
    clipboard.clear();
    if (session == nullptr)
        return;

    for (const auto item : currentSelection.items())
        if (item.kind == SelectionKind::clip)
            if (const auto info = clipInfo (item.ref); info.has_value())
                clipboard.push_back ({ *info, trackOf (item.ref), session->notes (item.ref) });
}

void ArrangementView::cutSelected()
{
    copySelected();
    if (! clipboard.empty())
    {
        if (session == nullptr)
            return;
        const auto clips = currentSelection.items();
        session->performAction (clips.size() == 1 ? "Cut Clip" : "Cut Clips",
                                [&] (auto& ops)
                                {
                                    for (const auto item : clips)
                                        if (item.kind == SelectionKind::clip)
                                            ops.deleteClip (item.ref);
                                });
        currentSelection.clear();
    }
}

std::vector<duet::model::ClipRef> ArrangementView::paste (double atBeats,
                                                          duet::model::TrackRef destinationTrack)
{
    std::vector<duet::model::ClipRef> pasted;
    if (session == nullptr || clipboard.empty() || destinationTrack == duet::model::noTrack)
        return pasted;

    const auto perSecond = beatsPerSecond();
    const auto earliest =
        std::min_element (clipboard.begin(),
                          clipboard.end(),
                          [] (const auto& left, const auto& right)
                          { return left.info.startSeconds < right.info.startSeconds; });
    const auto origin = earliest->info.startSeconds;

    session->performAction (
        "Paste Clips",
        [&] (auto& ops)
        {
            for (const auto& copied : clipboard)
            {
                const auto start = atBeats / perSecond + copied.info.startSeconds - origin;
                const auto rawStart = std::max (0.0, start - copied.info.contentOffsetSeconds);
                const auto rawLength = copied.info.lengthSeconds + copied.info.contentOffsetSeconds;
                auto clip = copied.info.holdsMidi
                                ? ops.insertMidiClip (
                                      destinationTrack, copied.info.name, rawStart, rawLength)
                                : ops.insertAudioClip (destinationTrack,
                                                       copied.info.name,
                                                       copied.info.sourceFile,
                                                       rawStart,
                                                       rawLength);
                if (copied.info.contentOffsetSeconds > 0.0)
                    ops.trimClip (clip, start, copied.info.lengthSeconds);
                for (const auto& note : copied.notes)
                    ops.addNote (
                        clip, note.pitch, note.startBeats, note.lengthBeats, note.velocity);
                if (copied.info.looped)
                    ops.setClipLoop (clip, true, copied.info.loopLengthBeats);
                if (copied.info.colour.has_value())
                    ops.setClipColour (clip, *copied.info.colour);
                pasted.push_back (clip);
            }
        });
    std::vector<SelectedItem> selected;
    selected.reserve (pasted.size());
    for (const auto clip : pasted)
        selected.push_back ({ SelectionKind::clip, clip });
    currentSelection.rubberBand (selected, false);
    return pasted;
}

void ArrangementView::duplicateSelected()
{
    if (session == nullptr)
        return;

    std::vector<duet::model::ClipRef> sources;
    for (const auto item : currentSelection.items())
        if (item.kind == SelectionKind::clip)
            sources.push_back (item.ref);
    if (sources.empty())
        return;

    std::vector<duet::model::ClipRef> copies;
    session->performAction (
        sources.size() == 1 ? "Duplicate Clip" : "Duplicate Clips",
        [&] (auto& ops)
        {
            for (const auto source : sources)
                if (const auto info = clipInfo (source); info.has_value())
                    copies.push_back (ops.duplicateClip (
                        source, trackOf (source), info->startSeconds + info->lengthSeconds));
        });
    std::vector<SelectedItem> selected;
    selected.reserve (copies.size());
    for (const auto copy : copies)
        selected.push_back ({ SelectionKind::clip, copy });
    currentSelection.rubberBand (selected, false);
}

void ArrangementView::renameSelectedClip (std::string name)
{
    if (session == nullptr || name.empty() || currentSelection.items().size() != 1)
        return;
    const auto item = currentSelection.items().front();
    if (item.kind == SelectionKind::clip)
        session->performAction ("Rename Clip",
                                [&] (auto& ops) { ops.renameClip (item.ref, name); });
}

void ArrangementView::setSelectedClipColour (duet::model::TrackColour colour)
{
    if (session == nullptr || currentSelection.empty())
        return;
    session->performAction ("Set Clip Colour",
                            [&] (auto& ops)
                            {
                                for (const auto item : currentSelection.items())
                                    if (item.kind == SelectionKind::clip)
                                        ops.setClipColour (item.ref, colour);
                            });
}

duet::model::ClipRef ArrangementView::createMidiClip (duet::model::TrackRef track, double atBeats)
{
    if (session == nullptr || session->track (track).kind != duet::model::TrackKind::midi)
        return duet::model::noClip;
    const auto grid = timeline.gridFor();
    const auto start = snapBeats (atBeats, grid, false);
    duet::model::ClipRef clip = duet::model::noClip;
    session->performAction ("Create MIDI Clip",
                            [&] (auto& ops)
                            {
                                clip =
                                    ops.insertMidiClip (track,
                                                        "MIDI Clip",
                                                        start / beatsPerSecond(),
                                                        grid.subdivisionBeats / beatsPerSecond());
                            });
    currentSelection.click ({ SelectionKind::clip, clip }, allClipItems(), false, false);
    return clip;
}

bool ArrangementView::isMidiClip (duet::model::ClipRef clip) const
{
    const auto info = clipInfo (clip);
    return info.has_value() && info->holdsMidi;
}

duet::model::ClipRef ArrangementView::selectedMidiClip() const
{
    for (const auto item : currentSelection.items())
        if (item.kind == SelectionKind::clip && isMidiClip (item.ref))
            return item.ref;
    return duet::model::noClip;
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

        case Command::selectAll:
            currentSelection.focus (SelectionKind::clip);
            currentSelection.selectAll (allClipItems());
            break;
        case Command::cut:
            cutSelected();
            break;
        case Command::copy:
            copySelected();
            break;
        case Command::paste:
            (void) paste (clock != nullptr ? clock->playheadBeats() : 0.0, focusedTrackRef);
            break;
        case Command::duplicate:
            duplicateSelected();
            break;
        case Command::deleteSelection:
            deleteSelected();
            break;
        case Command::cancel:
            cancelClipGesture();
            currentSelection.clear();
            break;

        default:
            // Rename needs a text editor, and every other command is another
            // surface's.
            break;
    }
}

//==============================================================================
int ArrangementView::playheadX() const
{
    return timeline.beatsToX (clock != nullptr ? clock->playheadBeats() : 0.0);
}

bool ArrangementView::isPlaying() const { return clock != nullptr && clock->isPlaying(); }

bool ArrangementView::followPlayback()
{
    if (! view.followPlayhead() || ! isPlaying() || clock == nullptr)
        return false;

    const auto x = playheadX();
    if (x >= 0 && x <= timeline.widthPx())
        return false;

    const auto leftMarginBeats =
        static_cast<double> (timeline.widthPx()) * 0.25 / view.hZoomPxPerBeat();
    view.setHScrollBeats (clock->playheadBeats() - leftMarginBeats);
    return true;
}

void ArrangementView::clickRuler (int px)
{
    if (clock != nullptr)
        clock->setPlayheadBeats (std::max (0.0, timeline.xToBeats (px)));
}
} // namespace duet::gui
