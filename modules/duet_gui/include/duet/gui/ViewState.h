#pragma once

#include <duet/model/Session.h>
#include <duet/persistence/DataNode.h>
#include <duet/persistence/Project.h>

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace duet::gui
{
/** Which of the bottom panel's two surfaces is in front. */
enum class BottomTab : std::uint8_t
{
    pianoRoll,
    mixer
};

/** The timeline snap spacing selected in the transport. */
enum class GridSize : std::uint8_t
{
    adaptive,
    quarter,
    eighth
};

/** How tall an automation lane is drawn, and how far a resize of one goes. A
    lane is shorter than a track row: several of them hang under one track and
    a curve is read at a glance rather than worked in.
*/
inline constexpr int defaultLaneHeightPx = 64;
inline constexpr int minimumLaneHeightPx = 28;
inline constexpr int maximumLaneHeightPx = 320;

/** One automation lane under a track: the curve it draws, and how tall it is
    drawn.

    Which curves a track's lanes show is the producer's view of their work and
    not the work itself — the points are the project's, the lanes looking at
    them are the view's — so a lane lives here, beside the row it hangs under,
    and goes when that row goes.
*/
struct LaneView
{
    duet::model::AutomationTarget target;
    int heightPx = defaultLaneHeightPx;
};

/** Where the interface is, for one project.

    Zoom, scroll, which docks are open and how wide, which bottom tab is in
    front, and a row for each track. It is the producer's view and not their
    work: it belongs to the project rather than to the app — reopening a project
    resumes where they left it — but it is not an edit, so moving a divider is
    never something an undo puts back and never what makes a project dirty. Spec
    535bbo settled both, ADR 0005 names the tree it lives in, and the persistence
    facade is what asks for it, once, as a save begins.

    Paintless, like every view-model in this module: it names no component and no
    JUCE type, and the shell reads it to lay itself out.

    Every measurement here is in pixels, exactly as the tree's attribute names
    say — a dock is as wide as the producer dragged it and a zoom is pixels per
    beat on the screen. The interface scale is not applied on top, because a
    zoom in pixels per beat is what the timeline's geometry is built on and a
    dock is whatever width the producer dragged it to. The defaults below are a
    window that reads well before the producer has dragged anything; from the
    first drag the numbers are theirs.
*/
class ViewState
{
public:
    ViewState() = default;

    //==============================================================================
    /** The name one track's row has under the VIEW tree, and the name one of
        its automation lanes has under that. The tree itself is named by the
        project format, at `duet::persistence::viewTreeName`.
    */
    static constexpr const char* trackTreeType = "TRACKVIEW";
    static constexpr const char* laneTreeType = "LANE";

    /** This view, as the project stores it. */
    [[nodiscard]] duet::persistence::DataNode toData() const;

    /** Takes the view a project was saved with. Anything the node does not
        carry keeps the value this view already had, and anything it carries
        that the interface cannot lay out is brought back into range — a project
        file is a text file, and one that has been edited by hand still has to
        open.
    */
    void readFrom (const duet::persistence::DataNode& stored);

    //==============================================================================
    [[nodiscard]] double hZoomPxPerBeat() const { return zoomPxPerBeat; }
    void setHZoomPxPerBeat (double newZoom);

    [[nodiscard]] double hScrollBeats() const { return scrollBeats; }
    void setHScrollBeats (double newScroll);

    [[nodiscard]] int vScrollPx() const { return scrollPx; }
    void setVScrollPx (int newScroll);

    [[nodiscard]] GridSize gridSize() const { return grid; }
    void setGridSize (GridSize newGrid) { grid = newGrid; }

    [[nodiscard]] bool followPlayhead() const { return followsPlayhead; }
    void setFollowPlayhead (bool shouldFollow) { followsPlayhead = shouldFollow; }

    [[nodiscard]] int pianoRollKeyHeightPx() const { return pianoKeyHeightPx; }
    void setPianoRollKeyHeightPx (int newHeight);

    [[nodiscard]] int pianoRollVScrollPx() const { return pianoVScrollPx; }
    void setPianoRollVScrollPx (int newScroll);

    //==============================================================================
    /** Whether a dock is open. A dock that is closed keeps the size it had, so
        that reopening it puts it back where the producer had it.
    */
    [[nodiscard]] bool browserVisible() const { return browserOpen; }
    void setBrowserVisible (bool shouldBeVisible) { browserOpen = shouldBeVisible; }

    [[nodiscard]] bool collaboratorVisible() const { return collaboratorOpen; }
    void setCollaboratorVisible (bool shouldBeVisible) { collaboratorOpen = shouldBeVisible; }

    [[nodiscard]] bool bottomVisible() const { return bottomOpen; }
    void setBottomVisible (bool shouldBeVisible) { bottomOpen = shouldBeVisible; }

    /** The sizes a divider drag sets. Each is held between the narrowest the
        dock is still usable at and the widest it is still a dock at.
    */
    [[nodiscard]] int browserWidthPx() const { return browserWidth; }
    void setBrowserWidthPx (int newWidth);

    [[nodiscard]] int collaboratorWidthPx() const { return collaboratorWidth; }
    void setCollaboratorWidthPx (int newWidth);

    [[nodiscard]] int bottomHeightPx() const { return bottomHeight; }
    void setBottomHeightPx (int newHeight);

    [[nodiscard]] BottomTab bottomTab() const { return tab; }
    void setBottomTab (BottomTab newTab) { tab = newTab; }

    //==============================================================================
    /** One track's row. A track the view has never been told about has the
        default row, so a track the producer has just added needs no entry.
    */
    [[nodiscard]] int trackHeightPx (duet::model::TrackRef track) const;
    void ensureTrack (duet::model::TrackRef track);
    void syncTracks (std::span<const duet::model::TrackRef> tracks);
    void removeTrack (duet::model::TrackRef track);
    [[nodiscard]] bool hasTrack (duet::model::TrackRef track) const;
    void setTrackHeightPx (duet::model::TrackRef track, int newHeight);

    /** Grows or shrinks every row at once, which is what a vertical zoom is:
        the timeline has no zoom of its own downwards — a track is as tall as the
        producer left it, and zooming is doing that to all of them together.
    */
    void scaleTrackHeights (double factor);

    [[nodiscard]] bool lanesExpanded (duet::model::TrackRef track) const;
    void setLanesExpanded (duet::model::TrackRef track, bool shouldBeExpanded);

    /** The automation lanes under one track, in the order they are drawn. A
        track nobody has opened the automation area of has none.
    */
    [[nodiscard]] std::vector<LaneView> lanes (duet::model::TrackRef track) const;
    void setLanes (duet::model::TrackRef track, std::vector<LaneView> newLanes);

    //==============================================================================
    static constexpr int defaultBrowserWidthPx = 340;
    static constexpr int minimumBrowserWidthPx = 220;
    static constexpr int maximumBrowserWidthPx = 640;

    static constexpr int defaultCollaboratorWidthPx = 440;
    static constexpr int minimumCollaboratorWidthPx = 280;
    static constexpr int maximumCollaboratorWidthPx = 760;

    static constexpr int defaultBottomHeightPx = 400;
    static constexpr int minimumBottomHeightPx = 180;
    static constexpr int maximumBottomHeightPx = 900;

    static constexpr int defaultTrackHeightPx = 84;
    static constexpr int minimumTrackHeightPx = 36;
    static constexpr int maximumTrackHeightPx = 480;

    static constexpr double defaultZoomPxPerBeat = 30.0;
    static constexpr double minimumZoomPxPerBeat = 1.0;
    static constexpr double maximumZoomPxPerBeat = 2000.0;
    static constexpr int defaultPianoRollKeyHeightPx = 14;
    static constexpr int minimumPianoRollKeyHeightPx = 6;
    static constexpr int maximumPianoRollKeyHeightPx = 64;

private:
    /** What one track's row holds. */
    struct TrackRow
    {
        int heightPx = defaultTrackHeightPx;
        bool lanesExpanded = false;
        std::vector<LaneView> lanes;
    };

    double zoomPxPerBeat = defaultZoomPxPerBeat;
    double scrollBeats = 0.0;
    int scrollPx = 0;
    GridSize grid = GridSize::adaptive;
    bool followsPlayhead = true;
    int pianoKeyHeightPx = defaultPianoRollKeyHeightPx;
    int pianoVScrollPx = 800;

    bool browserOpen = true;
    bool collaboratorOpen = true;
    bool bottomOpen = true;

    int browserWidth = defaultBrowserWidthPx;
    int collaboratorWidth = defaultCollaboratorWidthPx;
    int bottomHeight = defaultBottomHeightPx;
    BottomTab tab = BottomTab::pianoRoll;

    std::map<duet::model::TrackRef, TrackRow> trackRows;
};
} // namespace duet::gui
