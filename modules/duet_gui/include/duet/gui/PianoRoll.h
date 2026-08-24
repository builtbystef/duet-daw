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
    [[nodiscard]] std::vector<PianoNoteDrawing> notes() const;
    [[nodiscard]] std::vector<PianoKeyRow> rows() const;
    [[nodiscard]] PianoKeyRow rowForPitch (int pitch) const;

    [[nodiscard]] TimelineGeometry& geometry() { return timeline; }
    [[nodiscard]] const TimelineGeometry& geometry() const { return timeline; }
    void setWidthPx (int widthPx) { timeline.setWidthPx (widthPx); }
    [[nodiscard]] int playheadX() const;
    [[nodiscard]] double xToClipBeats (int x) const;

    void setNewNoteLengthBeats (double length);
    [[nodiscard]] double newNoteLengthBeats() const { return noteLengthBeats; }
    duet::model::NoteRef addNote (int pitch, double atBeats);
    void removeNote (duet::model::NoteRef note);

    void beginNoteGesture (duet::model::NoteRef note, NoteGestureKind kind);
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
    };

    [[nodiscard]] double gridBeats() const;
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
    double noteLengthBeats = 1.0;
    int scaleRoot = 0;
    Scale scale = Scale::chromatic;
    bool folded = false;
};
} // namespace duet::gui
