#include <duet/gui/PianoRoll.h>

#include <duet/gui/Snap.h>
#include <duet/gui/TimelineClock.h>
#include <duet/gui/ViewState.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>

namespace duet::gui
{
PianoRoll::PianoRoll (ViewState& projectView, Selection& sharedSelection)
    : view (projectView), selection (sharedSelection), timeline (view)
{
}

void PianoRoll::setSession (duet::model::Session* openProject)
{
    session = openProject;
    if (session == nullptr)
        close();
}

void PianoRoll::openClip (duet::model::ClipRef midiClip)
{
    if (session == nullptr)
        return;
    for (const auto& track : session->tracks())
        for (const auto& candidate : track.clips)
            if (candidate.clip == midiClip && candidate.holdsMidi)
            {
                clip = midiClip;
                selection.focus (SelectionKind::note);
                selection.clear();
                return;
            }
}

void PianoRoll::close()
{
    clip = duet::model::noClip;
    gesture.reset();
}

std::string PianoRoll::clipName() const
{
    if (session != nullptr)
        for (const auto& track : session->tracks())
            for (const auto& candidate : track.clips)
                if (candidate.clip == clip)
                    return candidate.name;
    return {};
}

std::vector<PianoNoteDrawing> PianoRoll::notes() const
{
    std::vector<PianoNoteDrawing> result;
    if (session == nullptr || clip == duet::model::noClip)
        return result;

    // Mid-gesture, the notes in hand are drawn at their destination: the delta
    // the drag has built so far, applied to every note it will land on.
    const auto deltaBeats =
        gesture.has_value() ? gesture->destinationBeats - gesture->original.startBeats : 0.0;
    const auto deltaPitch =
        gesture.has_value() ? gesture->destinationPitch - gesture->original.pitch : 0;
    std::set<duet::model::NoteRef> carried;
    if (gesture.has_value() && gesture->kind == NoteGestureKind::move)
        for (const auto& target : gestureTargets())
            carried.insert (target.note);

    for (auto note : session->notes (clip))
    {
        if (carried.contains (note.note))
        {
            note.startBeats = std::max (0.0, note.startBeats + deltaBeats);
            note.pitch = std::clamp (note.pitch + deltaPitch, minimumPitch, maximumPitch);
        }
        else if (gesture.has_value() && gesture->kind == NoteGestureKind::resizeRight
                 && note.note == gesture->original.note)
        {
            note.lengthBeats = std::max (gridBeats(), gesture->destinationBeats - note.startBeats);
        }

        const auto x = timeline.beatsToX (clipTimelineStartBeats() + note.startBeats);
        const auto right =
            timeline.beatsToX (clipTimelineStartBeats() + note.startBeats + note.lengthBeats);
        result.push_back ({ note.note,
                            note.pitch,
                            note.startBeats,
                            note.lengthBeats,
                            note.velocity,
                            x,
                            std::max (1, right - x),
                            rowForPitch (note.pitch).y });
    }
    return result;
}

std::vector<PianoKeyRow> PianoRoll::rows() const
{
    std::set<int, std::greater<>> used;
    if (folded && session != nullptr && clip != duet::model::noClip)
        for (const auto& note : session->notes (clip))
            used.insert (note.pitch);

    std::vector<PianoKeyRow> result;
    int y = folded ? 0 : -std::clamp (view.pianoRollVScrollPx(), 0, maximumVScrollPx());
    for (int pitch = maximumPitch; pitch >= minimumPitch; --pitch)
        if (! folded || used.contains (pitch))
        {
            result.push_back ({ pitch, y, keyHeightPx(), pitchInScale (pitch) });
            y += keyHeightPx();
        }
    return result;
}

PianoKeyRow PianoRoll::rowForPitch (int pitch) const
{
    const auto available = rows();
    const auto found = std::find_if (available.begin(),
                                     available.end(),
                                     [pitch] (const auto& row) { return row.pitch == pitch; });
    return found == available.end() ? PianoKeyRow { pitch, 0, keyHeightPx(), pitchInScale (pitch) }
                                    : *found;
}

int PianoRoll::playheadX() const
{
    return timeline.beatsToX (clock != nullptr ? clock->playheadBeats() : 0.0);
}

double PianoRoll::xToClipBeats (int x) const
{
    return timeline.xToBeats (x) - clipTimelineStartBeats();
}

void PianoRoll::setNewNoteLengthBeats (double length)
{
    noteLengthBeats = std::max (1.0 / 64.0, length);
}

duet::model::NoteRef PianoRoll::addNote (int pitch, double atBeats)
{
    if (session == nullptr || clip == duet::model::noClip)
        return duet::model::noNote;
    duet::model::NoteRef added = duet::model::noNote;

    // The floor, not the nearest line: the note lands in the grid cell the
    // producer clicked, never the next one over because the click sat past a
    // cell's midpoint.
    const auto start = std::max (0.0, std::floor (atBeats / gridBeats()) * gridBeats());
    session->performAction ("Add Note",
                            [&] (auto& ops)
                            {
                                added = ops.addNote (clip,
                                                     std::clamp (pitch, minimumPitch, maximumPitch),
                                                     start,
                                                     noteLengthBeats,
                                                     defaultVelocity);
                            });
    selection.click ({ SelectionKind::note, added }, allNoteItems(), false, false);
    return added;
}

void PianoRoll::removeNote (duet::model::NoteRef note)
{
    if (session == nullptr || ! noteInfo (note).has_value())
        return;
    session->performAction ("Remove Note", [&] (auto& ops) { ops.removeNote (note); });
    selection.clear();
}

void PianoRoll::beginNoteGesture (duet::model::NoteRef note,
                                  NoteGestureKind kind,
                                  double grabBeats,
                                  int grabPitch)
{
    if (const auto original = noteInfo (note); original.has_value())
        gesture = Gesture { *original,       kind,      original->startBeats,
                            original->pitch, grabBeats, grabPitch };
}

void PianoRoll::updateNoteGesture (double atBeats, int pitch, bool altHeld)
{
    if (! gesture.has_value())
        return;

    if (gesture->kind == NoteGestureKind::move)
    {
        // The note goes where the drag carries it, not where the pointer is:
        // keeping the pointer's offset into the note is what makes a note
        // grabbed by its middle land where it was let go.
        const auto carried = gesture->original.startBeats + (atBeats - gesture->grabBeats);
        gesture->destinationBeats =
            std::max (0.0, altHeld ? carried : std::round (carried / gridBeats()) * gridBeats());
        gesture->destinationPitch = std::clamp (
            gesture->original.pitch + (pitch - gesture->grabPitch), minimumPitch, maximumPitch);
    }
    else
    {
        gesture->destinationBeats =
            altHeld ? atBeats : std::round (atBeats / gridBeats()) * gridBeats();
        gesture->destinationPitch = gesture->original.pitch;
    }
}

bool PianoRoll::completeNoteGesture()
{
    if (session == nullptr || ! gesture.has_value())
        return false;
    const auto completed = *gesture;
    const auto targets = gestureTargets();
    gesture.reset();
    if (completed.kind == NoteGestureKind::move)
    {
        // One drag, one Action: the delta the grabbed note took carries every
        // selected note with it, and one undo brings them all back.
        const auto deltaBeats = completed.destinationBeats - completed.original.startBeats;
        const auto deltaPitch = completed.destinationPitch - completed.original.pitch;
        session->performAction (
            targets.size() == 1 ? "Move Note" : "Move Notes",
            [&] (auto& ops)
            {
                for (const auto& target : targets)
                    ops.moveNote (
                        target.note,
                        std::clamp (target.pitch + deltaPitch, minimumPitch, maximumPitch),
                        std::max (0.0, target.startBeats + deltaBeats));
            });
    }
    else
        session->performAction (
            "Resize Note",
            [&] (auto& ops)
            {
                const auto end = std::max (completed.original.startBeats + gridBeats(),
                                           completed.destinationBeats);
                ops.resizeNote (completed.original.note, end - completed.original.startBeats);
            });
    return true;
}

std::vector<duet::model::NoteInfo> PianoRoll::gestureTargets() const
{
    std::vector<duet::model::NoteInfo> targets;
    if (! gesture.has_value())
        return targets;
    for (const auto item : selection.items())
        if (item.kind == SelectionKind::note)
            if (const auto note = noteInfo (item.ref); note.has_value())
                targets.push_back (*note);
    const auto holdsGrabbed =
        std::any_of (targets.begin(),
                     targets.end(),
                     [this] (const auto& note) { return note.note == gesture->original.note; });
    if (! holdsGrabbed)
        if (const auto grabbed = noteInfo (gesture->original.note); grabbed.has_value())
            targets.push_back (*grabbed);
    return targets;
}

std::vector<SelectedItem> PianoRoll::allNoteItems() const
{
    std::vector<SelectedItem> result;
    if (session != nullptr && clip != duet::model::noClip)
        for (const auto& note : session->notes (clip))
            result.push_back ({ SelectionKind::note, note.note });
    return result;
}

bool PianoRoll::isNoteSelected (duet::model::NoteRef note) const
{
    return selection.contains ({ SelectionKind::note, note });
}

void PianoRoll::clickNote (duet::model::NoteRef note, bool ctrlHeld, bool shiftHeld)
{
    selection.focus (SelectionKind::note);
    selection.click ({ SelectionKind::note, note }, allNoteItems(), ctrlHeld, shiftHeld);
}

void PianoRoll::selectNotes (const std::vector<duet::model::NoteRef>& selected, bool ctrlHeld)
{
    std::vector<SelectedItem> items;
    items.reserve (selected.size());
    for (const auto note : selected)
        items.push_back ({ SelectionKind::note, note });
    selection.focus (SelectionKind::note);
    selection.rubberBand (items, ctrlHeld);
}

void PianoRoll::selectAll()
{
    selection.focus (SelectionKind::note);
    selection.selectAll (allNoteItems());
}

void PianoRoll::clearSelection() { selection.clear(); }

void PianoRoll::deleteSelected()
{
    if (session == nullptr)
        return;
    std::vector<duet::model::NoteRef> selected;
    for (const auto item : selection.items())
        if (item.kind == SelectionKind::note && noteInfo (item.ref).has_value())
            selected.push_back (item.ref);
    if (selected.empty())
        return;
    session->performAction (selected.size() == 1 ? "Delete Note" : "Delete Notes",
                            [&] (auto& ops)
                            {
                                for (const auto note : selected)
                                    ops.removeNote (note);
                            });
    selection.clear();
}

void PianoRoll::quantizeSelected()
{
    if (session == nullptr)
        return;
    std::vector<duet::model::NoteInfo> selected;
    for (const auto item : selection.items())
        if (item.kind == SelectionKind::note)
            if (const auto note = noteInfo (item.ref); note.has_value())
                selected.push_back (*note);
    if (selected.empty())
        return;
    session->performAction (
        selected.size() == 1 ? "Quantize Note" : "Quantize Notes",
        [&] (auto& ops)
        {
            for (const auto& note : selected)
                ops.moveNote (
                    note.note,
                    note.pitch,
                    std::max (0.0, std::round (note.startBeats / gridBeats()) * gridBeats()));
        });
}

void PianoRoll::setSelectedVelocity (duet::model::NoteRef grabbed, int velocity)
{
    if (session == nullptr)
        return;
    const auto original = noteInfo (grabbed);
    if (! original.has_value() || original->velocity <= 0)
        return;
    std::vector<duet::model::NoteInfo> selected;
    for (const auto item : selection.items())
        if (item.kind == SelectionKind::note)
            if (const auto note = noteInfo (item.ref); note.has_value())
                selected.push_back (*note);
    if (selected.empty())
        selected.push_back (*original);
    const auto ratio = static_cast<double> (std::clamp (velocity, 1, 127)) / original->velocity;
    session->performAction (
        selected.size() == 1 ? "Set Note Velocity" : "Scale Note Velocities",
        [&] (auto& ops)
        {
            for (const auto& note : selected)
                ops.setNoteVelocity (
                    note.note,
                    std::clamp (static_cast<int> (std::lround (note.velocity * ratio)), 1, 127));
        });
}

void PianoRoll::setScale (int rootPitchClass, Scale newScale)
{
    scaleRoot = ((rootPitchClass % 12) + 12) % 12;
    scale = newScale;
}

void PianoRoll::setKeyHeightPx (int height)
{
    view.setPianoRollKeyHeightPx (height);

    // Shorter keys make the whole roll shorter, and a scroll that was fine a
    // moment ago may now point past the last octave.
    view.setPianoRollVScrollPx (std::clamp (view.pianoRollVScrollPx(), 0, maximumVScrollPx()));
}

int PianoRoll::keyHeightPx() const { return view.pianoRollKeyHeightPx(); }
void PianoRoll::verticalZoom (double factor)
{
    setKeyHeightPx (static_cast<int> (std::lround (keyHeightPx() * factor)));
}

void PianoRoll::scrollVertically (int pixels)
{
    view.setPianoRollVScrollPx (
        std::clamp (view.pianoRollVScrollPx() + pixels, 0, maximumVScrollPx()));
}

void PianoRoll::setHeightPx (int heightPx)
{
    gridHeightPx = std::max (0, heightPx);
    view.setPianoRollVScrollPx (std::clamp (view.pianoRollVScrollPx(), 0, maximumVScrollPx()));
}

int PianoRoll::maximumVScrollPx() const
{
    // The whole roll, top octave to bottom, less the window it is seen
    // through. A window taller than the roll leaves nothing to scroll.
    return std::max (0, (maximumPitch - minimumPitch + 1) * keyHeightPx() - gridHeightPx);
}

double PianoRoll::gridBeats() const
{
    if (view.gridSize() == GridSize::quarter)
        return 1.0;
    if (view.gridSize() == GridSize::eighth)
        return 0.5;
    return timeline.gridFor().subdivisionBeats;
}

std::optional<duet::model::NoteInfo> PianoRoll::noteInfo (duet::model::NoteRef wanted) const
{
    if (session != nullptr && clip != duet::model::noClip)
        for (const auto& note : session->notes (clip))
            if (note.note == wanted)
                return note;
    return {};
}

bool PianoRoll::pitchInScale (int pitch) const
{
    if (scale == Scale::chromatic)
        return true;
    constexpr std::array major { 0, 2, 4, 5, 7, 9, 11 };
    constexpr std::array minor { 0, 2, 3, 5, 7, 8, 10 };
    const auto interval = ((pitch - scaleRoot) % 12 + 12) % 12;
    const auto& intervals = scale == Scale::major ? major : minor;
    return std::find (intervals.begin(), intervals.end(), interval) != intervals.end();
}

double PianoRoll::clipTimelineStartBeats() const
{
    if (session != nullptr)
        for (const auto& track : session->tracks())
            for (const auto& candidate : track.clips)
                if (candidate.clip == clip)
                    return (candidate.startSeconds - candidate.contentOffsetSeconds)
                           * std::max (1.0, session->tempoBpm()) / 60.0;
    return 0.0;
}
} // namespace duet::gui
