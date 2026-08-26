#pragma once

#include <duet/gui/TimelineGeometry.h>

#include <duet/model/Session.h>

#include <optional>
#include <string>
#include <vector>

namespace duet::gui
{
class ViewState;

/** One curve a lane can be bound to, as its picker offers it: what it drives,
    what the producer calls it, and the range it moves in.

    The range is the target's own — decibels for a fader, −1 to +1 for a pan, a
    parameter's own units for a plugin's — which is what makes a lane's height
    mean the same thing as the control the curve drives.
*/
struct AutomationTargetOption
{
    duet::model::AutomationTarget target;
    std::string name;
    double minimumValue = 0.0;
    double maximumValue = 1.0;

    /** How the target's value is spread over that range: the proportion of the
        range raised to this is the height up the lane, so one draws it evenly
        and a skew below one lifts the small values away from the floor. It is
        the target's own — a fader, a pan and every parameter already even in
        the producer's ear are one; a compressor's ratio, whose range reaches
        1000 to one, is not.
    */
    double skew = 1.0;
};

/** One point of a curve, where the lane draws it. */
struct AutomationPointDrawing
{
    double beats = 0.0;
    double value = 0.0;

    /** How the segment leaving this point bends, straight through milestone
        one. A gesture carries it back to the model untouched, so that the
        curvature a later milestone draws is never lost by being moved.
    */
    double curvature = 0.0;

    /** Where the point is, in pixels: `x` from the timeline's left edge like a
        clip's, `y` from the top of the track's automation area.
    */
    int x = 0;
    int y = 0;
};

/** One automation lane under a track, where the arrangement draws it. */
struct AutomationLaneDrawing
{
    duet::model::TrackRef track = duet::model::noTrack;

    /** Which lane of that track's automation area this is. */
    int index = 0;

    duet::model::AutomationTarget target;
    std::string targetName;
    double minimumValue = 0.0;
    double maximumValue = 1.0;

    /** How the value is spread over that range, as the picker offered it. */
    double skew = 1.0;

    /** Where the lane is, in pixels from the top of the track's automation
        area: the arrangement puts that area under the track's row.
    */
    int y = 0;
    int height = 0;

    std::vector<AutomationPointDrawing> points;
};

/** What value a height in a lane stands for, and where a value is drawn.

    A lane's top is its target's largest value and its bottom its smallest, as
    every control the producer drags upwards to raise is. The lane's skew is how
    the values in between are spread over the height, and the pair applies it in
    both directions, so a point is grabbed where it was drawn.

    Pure geometry: the pair is the whole of what a lane's height means, so a
    gesture and the paint that follows it read the same lane the same way.
*/
[[nodiscard]] double valueAtY (const AutomationLaneDrawing& lane, int y);
[[nodiscard]] int yForValue (const AutomationLaneDrawing& lane, double value);

/** Which of a lane's points the pointer is on, or nothing where it is on the
    lane and not on a point. A point is a small square and the pointer is not
    precise, so the reach is `pointHitRadiusPx` around it.
*/
[[nodiscard]] std::optional<int> pointIndexAt (const AutomationLaneDrawing& lane, int x, int y);

/** How far from a point the pointer still counts as being on it, in pixels. */
inline constexpr int pointHitRadiusPx = 6;

/** The automation lanes under the arrangement's tracks, without the painting.

    A track's header opens an area beneath it, and each lane in that area is
    bound to one curve — the track's volume or pan, or a parameter of one of its
    plugins — chosen from the lane's own picker. Lanes draw on the arrangement's
    own geometry, so the grid, the zoom and the scroll are the ones the clips
    above them are drawn on.

    Which lanes a track has open, and what each is bound to, is the producer's
    view rather than their work: it lives in the view state, is captured by a
    save, and is on no undo stack. The points themselves are the project's, and
    every gesture here ends in one named Action (ADR 0004).

    Milestone one draws straight segments between points. Nothing on the way
    from a gesture to the model assumes that: a point's curvature is carried
    through every gesture untouched, so the curves of a later milestone are a
    value that changes rather than data that has to be rewritten.
*/
class AutomationLanes
{
public:
    /** @param projectView  the view state the lanes and the zoom live in
        @param sharedTimeline  the arrangement's geometry, shared so that a lane
                               and the clips above it count beats alike
    */
    AutomationLanes (ViewState& projectView, TimelineGeometry& sharedTimeline);

    ~AutomationLanes() = default;

    AutomationLanes (const AutomationLanes& other) = delete;
    AutomationLanes& operator= (const AutomationLanes& other) = delete;

    /** The project whose curves these lanes draw, or nothing while none is
        open.
    */
    void setSession (duet::model::Session* openProject);

    //==============================================================================
    /** Whether a track's automation area is open. Opening one for the first
        time gives it a lane on the track's own fader, which is the curve a
        producer reaches for first.
    */
    [[nodiscard]] bool expanded (duet::model::TrackRef track) const;
    void setExpanded (duet::model::TrackRef track, bool shouldBeExpanded);
    void toggleExpanded (duet::model::TrackRef track);

    /** How tall a track's whole automation area is, and zero for a track whose
        area is closed.
    */
    [[nodiscard]] int heightPx (duet::model::TrackRef track) const;

    /** The lanes of one track, in the order they are drawn, and none at all
        while the area is closed.
    */
    [[nodiscard]] std::vector<AutomationLaneDrawing> lanes (duet::model::TrackRef track) const;

    /** What a lane's curve is worth at a beat: the value between two points,
        joined by the straight segment milestone one draws, and the value of the
        nearest point beyond either end. A lane with no points on it reads back
        what the target plays where no curve takes it over.
    */
    [[nodiscard]] double
        valueAtBeats (duet::model::TrackRef track, int laneIndex, double beats) const;

    //==============================================================================
    /** What a lane's picker offers for one track: its volume, its pan, and every
        parameter of every plugin in its chain.
    */
    [[nodiscard]] std::vector<AutomationTargetOption>
        targetsFor (duet::model::TrackRef track) const;

    void setLaneTarget (duet::model::TrackRef track,
                        int laneIndex,
                        const duet::model::AutomationTarget& target);

    //==============================================================================
    /** Puts a point on a lane's curve where the producer double-clicked: on the
        grid unless Alt is held, and at whatever value that height stands for —
        a value is continuous and never snaps. One Action.
    */
    void addPoint (duet::model::TrackRef track, int laneIndex, double atBeats, int y, bool altHeld);

    /** Takes the point under the pointer off the curve, as one Action, and
        answers whether there was one. Empty lane space is not a point, and
        right-clicking it changes nothing.
    */
    bool removePoint (duet::model::TrackRef track, int laneIndex, int x, int y);

    //==============================================================================
    /** Dragging one point. The whole drag is one Action, emitted at the end;
        a drag abandoned with Escape emits none. Horizontal movement snaps to
        the grid unless Alt is held at the moment the drag is read, and the
        value moves freely — it is continuous and has no grid to land on.
    */
    void beginPointGesture (duet::model::TrackRef track, int laneIndex, int pointIndex);
    void updatePointGesture (double atBeats, int y, bool altHeld);
    [[nodiscard]] bool completePointGesture();
    void cancelPointGesture();
    [[nodiscard]] bool hasPointGesture() const { return gesture.has_value(); }

    /** Adds a lane under a track, and returns which lane it is. */
    int addLane (duet::model::TrackRef track);
    void removeLane (duet::model::TrackRef track, int laneIndex);

private:
    /** A drag in progress: which point of which lane, where it started, and
        where it has been dragged to.
    */
    struct Gesture
    {
        duet::model::AutomationTarget target;
        double originalSeconds = 0.0;
        double curvature = 0.0;
        double destinationBeats = 0.0;
        double value = 0.0;

        /** The lane the point is being dragged in, as it was when the drag
            began: what a height means is the lane's, and a drag reads it
            while the pointer moves.
        */
        AutomationLaneDrawing lane;
    };

    [[nodiscard]] double beatsPerSecond() const;
    [[nodiscard]] AutomationTargetOption optionFor (const duet::model::AutomationTarget& target,
                                                    duet::model::TrackRef track) const;

    ViewState& view;
    TimelineGeometry& timeline;
    duet::model::Session* session = nullptr;
    std::optional<Gesture> gesture;
};
} // namespace duet::gui
