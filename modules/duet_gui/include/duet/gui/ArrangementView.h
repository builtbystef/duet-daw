#pragma once

#include <duet/gui/Shortcuts.h>
#include <duet/gui/TimelineGeometry.h>

#include <duet/model/Session.h>

#include <filesystem>
#include <string>
#include <vector>

namespace duet::gui
{
class TimelineClock;
class ViewState;

/** A wheel or trackpad gesture, as the interface's conventions see it.

    No JUCE type crosses this seam, so what a gesture does can be asserted with
    no window on screen.
*/
struct NoteDrawing
{
    int x = 0;
    int width = 0;
    int pitch = 0;
};

struct ClipDrawing
{
    duet::model::ClipRef clip = duet::model::noClip;
    std::string name;
    int x = 0;
    int width = 0;
    bool holdsMidi = false;
    std::filesystem::path sourceFile;
    std::vector<NoteDrawing> notes;
};

struct TrackDrawing
{
    duet::model::TrackRef track = duet::model::noTrack;
    std::string name;
    duet::model::TrackKind kind = duet::model::TrackKind::audio;
    duet::model::TrackColour colour = duet::model::TrackColour::orange;
    bool muted = false;
    bool soloed = false;
    bool recordArmed = false;
    int y = 0;
    int height = 0;
    std::vector<ClipDrawing> clips;
};

struct ScrollGesture
{
    /** How far the wheel turned, in notches. Away from the producer is
        positive, which is up on the screen and in on a zoom.
    */
    double deltaX = 0.0;
    double deltaY = 0.0;

    /** Where the pointer is, in pixels from the canvas's left edge: what a zoom
        is anchored at.
    */
    int pointerX = 0;

    bool ctrl = false;
    bool shift = false;
};

/** The arrangement surface, without the painting.

    It owns the timeline's geometry, holds the conventions the whole app shares
    for scrolling and zooming (spec 535bbo), and knows where the playhead is. The
    canvas above it paints what this says and hands it the producer's gestures;
    everything a test needs to ask is here.

    Where the producer has scrolled and zoomed to is the project's, so this keeps
    no copy of it: the view state is the one place it lives, and a save captures
    it from there.
*/
class ArrangementView
{
public:
    /** @param projectView  the layout of the open project */
    explicit ArrangementView (ViewState& projectView);

    ~ArrangementView() = default;

    /** One arrangement belongs to one view state and one surface. */
    ArrangementView (const ArrangementView& other) = delete;
    ArrangementView& operator= (const ArrangementView& other) = delete;

    //==============================================================================
    /** The clock of the project open in this arrangement, or nothing when none
        is. It is read and never held past its project.
    */
    void setClock (TimelineClock* projectClock);

    /** The project whose tracks this surface presents, or nothing while no
        project is open. */
    void setSession (duet::model::Session* openProject);

    /** Takes the project's metre, which is what the grid counts bars in. The
        canvas calls this before it draws, so that a time signature the producer
        has just changed is the one on screen.
    */
    void refresh();

    //==============================================================================
    /** How wide the canvas is: what decides which lines are on screen, and what
        a zoom about the centre and a zoom-to-fit are measured against. How tall
        it is matters to nothing until there are tracks down it (issue s1jzd4).
    */
    void setWidthPx (int widthPx);
    void setHeightPx (int heightPx);

    /** Track rows and clips in canvas coordinates, freshly read from the model. */
    [[nodiscard]] std::vector<TrackDrawing> tracks();

    [[nodiscard]] int contentHeightPx() const;
    [[nodiscard]] int addTrackRowY() const;

    //==============================================================================
    /** Producer gestures at the Action seam. */
    duet::model::TrackRef addTrack (duet::model::TrackKind kind);
    void reorderTrack (duet::model::TrackRef track, int newIndex);
    void renameTrack (duet::model::TrackRef track, std::string name);
    duet::model::TrackRef duplicateTrack (duet::model::TrackRef track);
    void deleteTrack (duet::model::TrackRef track);
    void setTrackColour (duet::model::TrackRef track, duet::model::TrackColour colour);
    void toggleMute (duet::model::TrackRef track);
    void toggleSolo (duet::model::TrackRef track);
    void toggleRecordArm (duet::model::TrackRef track);
    void resizeTrack (duet::model::TrackRef track, int heightPx);

    [[nodiscard]] TimelineGeometry& geometry() { return timeline; }
    [[nodiscard]] const TimelineGeometry& geometry() const { return timeline; }

    //==============================================================================
    /** What a wheel gesture means, by the conventions every surface shares:
        plain is vertical, Shift is horizontal, Ctrl zooms the timeline about the
        pointer, and Ctrl with Shift zooms the tracks' heights.
    */
    void scroll (const ScrollGesture& gesture);

    /** What a zoom key means. Anything that is not a zoom is not this surface's,
        and is ignored.
    */
    void perform (Command command);

    //==============================================================================
    /** Where the playhead is, in pixels from the canvas's left edge. Off the
        canvas either way when the transport is somewhere the producer is not
        looking.
    */
    [[nodiscard]] int playheadX() const;

    /** Whether the transport is rolling, which is what decides whether the
        canvas keeps asking where the playhead is.
    */
    [[nodiscard]] bool isPlaying() const;

    /** A click on the ruler: the playhead lands where it was clicked. Never an
        Action — moving the playhead is not a change to the project — and never
        left of the start of the project.
    */
    void clickRuler (int px);

    //==============================================================================
    /** How far one notch of a wheel scrolls, in pixels, and how much of a zoom
        one notch is.
    */
    static constexpr int scrollPixelsPerNotch = 60;
    static constexpr int addTrackRowHeightPx = 36;
    static constexpr double zoomFactorPerNotch = 1.25;

    /** How much of a zoom one press of a zoom key is. More than a wheel notch,
        because a key is pressed once and a wheel is turned.
    */
    static constexpr double zoomFactorPerKeyPress = 1.5;

private:
    ViewState& view;
    TimelineGeometry timeline { view };
    TimelineClock* clock = nullptr;
    duet::model::Session* session = nullptr;
    int heightPx = 0;
};
} // namespace duet::gui
