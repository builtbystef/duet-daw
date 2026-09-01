#pragma once

#include <duet/gui/Selection.h>
#include <duet/gui/TimelineGeometry.h>

#include <duet/model/Session.h>

#include <optional>
#include <string>
#include <vector>

namespace duet::gui
{
class TimelineClock;
class ViewState;

enum class Scale : std::uint8_t
{
    chromatic,
    major,
    minor
};

enum class NoteGestureKind : std::uint8_t
{
    move,
    resizeRight
};

struct PianoKeyRow
{
    int pitch = 0;
    int y = 0;
    int height = 0;
    bool inScale = true;
};

struct PianoNoteDrawing
{
    duet::model::NoteRef note = duet::model::noNote;
    int pitch = 0;
    double startBeats = 0.0;
    double lengthBeats = 0.0;
    int velocity = 0;
    int x = 0;
    int width = 0;
    int y = 0;
};

/** The paintless MIDI-note editor for the clip open in the Piano Roll. */
class PianoRoll
{
public:
    PianoRoll (ViewState& projectView, Selection& sharedSelection);

    void setSession (duet::model::Session* openProject);
    void setClock (TimelineClock* projectClock) { clock = projectClock; }
    void openClip (duet::model::ClipRef midiClip);
    void close();

    [[nodiscard]] bool isOpen() const { return clip != duet::model::noClip; }
    [[nodiscard]] duet::model::ClipRef openClipRef() const { return clip; }
    [[nodiscard]] std::string clipName() const;

    /** Every note, where it is drawn right now — which, mid-gesture, is where
        the drag has carried it, not where the model still holds it: the notes
        being moved and the edge being pulled follow the pointer, snapped, so
        the producer watches the result they will get and not a stand-in.
    */
    [[nodiscard]] std::vector<PianoNoteDrawing> notes() const;
    [[nodiscard]] std::vector<PianoKeyRow> rows() const;
    [[nodiscard]] PianoKeyRow rowForPitch (int pitch) const;

    [[nodiscard]] TimelineGeometry& geometry() { return timeline; }
    [[nodiscard]] const TimelineGeometry& geometry() const { return timeline; }
    void setWidthPx (int widthPx) { timeline.setWidthPx (widthPx); }

    /** How tall the area the rows are drawn into is. With the height known the
        roll ends exactly at its octaves: the scroll stops where the highest
        pitch meets the top edge and where the lowest meets the bottom, and no
        empty space beyond either can be scrolled into view.
    */
    void setHeightPx (int heightPx);
    [[nodiscard]] int playheadX() const;
    [[nodiscard]] double xToClipBeats (int x) const;

    void setNewNoteLengthBeats (double length);
    [[nodiscard]] double newNoteLengthBeats() const { return noteLengthBeats; }
    duet::model::NoteRef addNote (int pitch, double atBeats);
    void removeNote (duet::model::NoteRef note);

    /** Starts a drag on a note, remembering where in it the producer took
        hold: the beats and pitch under the pointer at mouse-down. A move keeps
        that offset — a note grabbed by its middle lands where it was let go,
        not wherever its start jumps to under the pointer — and a move carries
        the whole selection the grabbed note is part of.
    */
    void beginNoteGesture (duet::model::NoteRef note,
                           NoteGestureKind kind,
                           double grabBeats,
                           int grabPitch);
    void updateNoteGesture (double atBeats, int pitch, bool altHeld);
    [[nodiscard]] bool completeNoteGesture();
    void cancelNoteGesture() { gesture.reset(); }

    [[nodiscard]] std::vector<SelectedItem> allNoteItems() const;
    [[nodiscard]] bool isNoteSelected (duet::model::NoteRef note) const;
    void clickNote (duet::model::NoteRef note, bool ctrlHeld, bool shiftHeld);
    void selectNotes (const std::vector<duet::model::NoteRef>& selected, bool ctrlHeld);
    void selectAll();
    void clearSelection();
    void deleteSelected();
    void quantizeSelected();
    void setSelectedVelocity (duet::model::NoteRef grabbed, int velocity);

    void setScale (int rootPitchClass, Scale newScale);
    void setFolded (bool shouldFold) { folded = shouldFold; }
    [[nodiscard]] bool isFolded() const { return folded; }

    void setKeyHeightPx (int height);
    [[nodiscard]] int keyHeightPx() const;
    void verticalZoom (double factor);
    void scrollVertically (int pixels);

    static constexpr int defaultVelocity = 100;
    static constexpr int minimumPitch = 0;
    static constexpr int maximumPitch = 127;

private:
    struct Gesture
    {
        duet::model::NoteInfo original;
        NoteGestureKind kind = NoteGestureKind::move;
        double destinationBeats = 0.0;
        int destinationPitch = 0;
        double grabBeats = 0.0;
        int grabPitch = 0;
    };

    [[nodiscard]] double gridBeats() const;
    [[nodiscard]] int maximumVScrollPx() const;
    [[nodiscard]] std::vector<duet::model::NoteInfo> gestureTargets() const;
    [[nodiscard]] std::optional<duet::model::NoteInfo> noteInfo (duet::model::NoteRef wanted) const;
    [[nodiscard]] bool pitchInScale (int pitch) const;
    [[nodiscard]] double clipTimelineStartBeats() const;

    ViewState& view;
    Selection& selection;
    TimelineGeometry timeline;
    duet::model::Session* session = nullptr;
    TimelineClock* clock = nullptr;
    duet::model::ClipRef clip = duet::model::noClip;
    std::optional<Gesture> gesture;
    int gridHeightPx = 0;
    double noteLengthBeats = 1.0;
    int scaleRoot = 0;
    Scale scale = Scale::chromatic;
    bool folded = false;
};
} // namespace duet::gui
