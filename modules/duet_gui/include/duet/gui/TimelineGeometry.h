#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace duet::gui
{
class ViewState;

/** How strongly one grid line is drawn: the three weights the token set has a
    colour for.
*/
enum class GridWeight : std::uint8_t
{
    bar,
    beat,
    fine
};

/** One line of the grid, where the canvas draws it. */
struct GridLine
{
    int x = 0;
    double beats = 0.0;
    GridWeight weight = GridWeight::fine;
};

/** One of the ruler's labels: a bar as "3", a beat inside a bar as "3.2". */
struct RulerLabel
{
    int x = 0;
    double beats = 0.0;
    std::string text;
};

/** The lines one zoom has room for.

    The subdivision is both what the timeline draws and what a gesture snaps to,
    which is why one value answers both: a producer dragging a clip lands on the
    lines they can see.
*/
struct GridSpec
{
    /** Beats between two lines. A quarter — a 1/16 note — where the zoom has
        room for one, several bars where it barely has room for a bar.
    */
    double subdivisionBeats = 1.0;

    /** How many beats a bar is, which is what the project's time signature
        says.
    */
    double barBeats = 4.0;
};

/** The arrangement's coordinate system: where a beat is on the screen, which
    grid the zoom has room for, and what a zoom gesture does to both.

    Every surface that draws musical time reads its geometry from one of these —
    the arrangement now, the piano roll and the automation lanes later — so the
    mapping is written once and the grid is the same grid everywhere.

    The zoom and the horizontal scroll are the producer's and belong to the
    project, so this holds no copy of them: it reads and writes the view state,
    which is what a save captures and what reopening a project restores (spec
    535bbo). Paintless, like every view-model in this module.
*/
class TimelineGeometry
{
public:
    /** @param projectView  the view state the zoom and the scroll live in */
    explicit TimelineGeometry (ViewState& projectView);

    ~TimelineGeometry() = default;

    /** One geometry belongs to one view state and one surface. Copying it would
        be two things writing one zoom.
    */
    TimelineGeometry (const TimelineGeometry& other) = delete;
    TimelineGeometry& operator= (const TimelineGeometry& other) = delete;

    //==============================================================================
    /** The beat at a pixel, counting from the canvas's left edge. */
    [[nodiscard]] double xToBeats (int px) const;

    /** Where a beat is, in pixels from the canvas's left edge. A beat left of
        the scroll position is a negative pixel: it is off the canvas, not at
        its edge.
    */
    [[nodiscard]] int beatsToX (double beats) const;

    //==============================================================================
    /** How many beats a bar is: the project's time signature, in the unit the
        timeline counts in. Four, until a project says otherwise.
    */
    void setBeatsPerBar (double beats);
    [[nodiscard]] double beatsPerBar() const { return barBeats; }

    /** The finest subdivision whose lines are at least `minimumGridSpacingPx`
        apart at the zoom in force, and never finer than a 1/64 note or coarser
        than sixteen bars.
    */
    [[nodiscard]] GridSpec gridFor() const;

    //==============================================================================
    /** How wide the canvas is. It is what decides which lines are on screen,
        what a zoom about the centre is about, and what zoom-to-fit fits into.
    */
    void setWidthPx (int newWidth);
    [[nodiscard]] int widthPx() const { return canvasWidth; }

    /** The lines on screen, left to right: every multiple of the grid's
        subdivision, each weighted by what it is — the start of a bar, a beat,
        or one of the subdivisions between them.
    */
    [[nodiscard]] std::vector<GridLine> gridLines() const;

    /** The ruler's labels, left to right. Bars are numbered from one, as the
        producer counts them, and the beats inside a bar are labelled only where
        the zoom leaves room to read them.
    */
    [[nodiscard]] std::vector<RulerLabel> rulerLabels() const;

    //==============================================================================
    /** Zooms about a pointer: the beat under `anchorPx` is the beat under it
        afterwards, so a producer zooming in on a phrase keeps the phrase under
        the mouse. A factor above one zooms in.

        The one exception is the left edge of the project: a timeline never
        scrolls left of beat 0, so zooming out far enough puts beat 0 at the
        canvas's edge and the anchor moves with it.
    */
    void zoomAt (int anchorPx, double factor);

    /** Zooms about the middle of the canvas, which is what the zoom keys do:
        there is no pointer to anchor to, and the middle is what the producer is
        looking at.
    */
    void zoomAtCentre (double factor);

    /** Puts a project's whole content across the canvas, from its start. What
        the zoom-to-fit key does; a project with nothing in it is left at a zoom
        that can be worked at rather than at the end of the range.
    */
    void fitToWidth (double contentLengthBeats);

    /** Scrolls sideways by a distance on the screen. The timeline never scrolls
        left of beat 0 — there is no project there.
    */
    void scrollByPixels (int px);

    //==============================================================================
    /** How close together the timeline will draw two lines. Below this the
        grid steps up to the next subdivision: the number the r4m858 prototype
        settled by eye, and the one spec 535bbo records.
    */
    static constexpr double minimumGridSpacingPx = 18.0;

    /** The finest and coarsest grids the timeline offers: a 1/64 note, and the
        sixteen bars a zoomed-out arrangement is read in. The view state's zoom
        range keeps both reachable.
    */
    static constexpr double finestSubdivisionBeats = 1.0 / 16.0;
    static constexpr double coarsestSubdivisionBars = 16.0;

    /** How much room a ruler label needs. Wider than a grid line needs, because
        a label is read and a line is only seen: the ruler labels the coarser of
        the two grids, and never labels anything finer than a beat.
    */
    static constexpr double minimumLabelSpacingPx = 48.0;

private:
    /** The finest rung of the grid's ladder whose lines are at least that far
        apart, in beats. `neverFinerThanABeat` is what the ruler asks with.
    */
    [[nodiscard]] double subdivisionFor (double minimumSpacingPx, bool neverFinerThanABeat) const;

    ViewState& view;
    double barBeats = 4.0;
    int canvasWidth = 0;
};
} // namespace duet::gui
