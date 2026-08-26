#include <duet/gui/AutomationLanes.h>

#include <duet/gui/Mixer.h>
#include <duet/gui/Snap.h>
#include <duet/gui/ViewState.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace duet::gui
{
namespace
{
    /** What the picker calls the two curves every track is born with. */
    constexpr const char* volumeOptionName = "Volume";
    constexpr const char* panOptionName = "Pan";

    /** How far a pan curve moves: hard left to hard right, as the model writes
        it.
    */
    constexpr double panMinimum = -1.0;
    constexpr double panMaximum = 1.0;

    [[nodiscard]] bool sameTarget (const duet::model::AutomationTarget& one,
                                   const duet::model::AutomationTarget& other)
    {
        return one.kind == other.kind && one.track == other.track && one.plugin == other.plugin
               && one.parameterId == other.parameterId;
    }

    /** A lane's skew, held to the positive numbers the exponent is defined for:
        a lane arriving with nothing said about its skew, or with a number no
        control could draw, is the even one every target but a handful is.
    */
    [[nodiscard]] double skewOf (const AutomationLaneDrawing& lane)
    {
        return lane.skew > 0.0 ? lane.skew : 1.0;
    }

    /** How far up its range a value sits, from 0 at the lane's bottom to 1 at
        its top, and the value at such a height.

        The skew is the exponent between the two: the height is the proportion
        of the range raised to it, which is how a control that draws a parameter
        spreads it. One leaves the proportion alone, so a lane that says nothing
        about its skew is the plain linear lane it has always been.
    */
    [[nodiscard]] double heightOfValue (const AutomationLaneDrawing& lane, double value)
    {
        const auto proportion = std::clamp (
            (value - lane.minimumValue) / (lane.maximumValue - lane.minimumValue), 0.0, 1.0);
        const auto skew = skewOf (lane);

        return skew == 1.0 ? proportion : std::pow (proportion, skew);
    }

    [[nodiscard]] double valueAtHeight (const AutomationLaneDrawing& lane, double fromBottom)
    {
        const auto held = std::clamp (fromBottom, 0.0, 1.0);
        const auto skew = skewOf (lane);
        const auto proportion = skew == 1.0 ? held : std::pow (held, 1.0 / skew);

        return lane.minimumValue + proportion * (lane.maximumValue - lane.minimumValue);
    }
} // namespace

double valueAtY (const AutomationLaneDrawing& lane, int y)
{
    if (lane.height <= 0 || lane.maximumValue <= lane.minimumValue)
        return lane.maximumValue;

    const auto fromTop = static_cast<double> (y - lane.y) / static_cast<double> (lane.height);

    return std::clamp (valueAtHeight (lane, 1.0 - fromTop), lane.minimumValue, lane.maximumValue);
}

int yForValue (const AutomationLaneDrawing& lane, double value)
{
    if (lane.height <= 0 || lane.maximumValue <= lane.minimumValue)
        return lane.y;

    const auto fromTop = 1.0 - heightOfValue (lane, value);

    return lane.y
           + std::clamp (
               static_cast<int> (std::lround (fromTop * lane.height)), 0, lane.height - 1);
}

std::optional<int> pointIndexAt (const AutomationLaneDrawing& lane, int x, int y)
{
    for (std::size_t point = 0; point < lane.points.size(); ++point)
    {
        const auto& candidate = lane.points[point];

        if (std::abs (candidate.x - x) <= pointHitRadiusPx
            && std::abs (candidate.y - y) <= pointHitRadiusPx)
            return static_cast<int> (point);
    }

    return std::nullopt;
}

AutomationLanes::AutomationLanes (ViewState& projectView, TimelineGeometry& sharedTimeline)
    : view (projectView), timeline (sharedTimeline)
{
}

void AutomationLanes::setSession (duet::model::Session* openProject) { session = openProject; }

//==============================================================================
bool AutomationLanes::expanded (duet::model::TrackRef track) const
{
    return view.lanesExpanded (track);
}

void AutomationLanes::setExpanded (duet::model::TrackRef track, bool shouldBeExpanded)
{
    if (track == duet::model::noTrack)
        return;

    view.setLanesExpanded (track, shouldBeExpanded);

    // An area opened for the first time is an area with nothing in it, which
    // says nothing about the track. The fader is the curve a producer reaches
    // for first, so that is what the first lane is bound to.
    if (shouldBeExpanded && view.lanes (track).empty())
        (void) addLane (track);
}

void AutomationLanes::toggleExpanded (duet::model::TrackRef track)
{
    setExpanded (track, ! expanded (track));
}

int AutomationLanes::heightPx (duet::model::TrackRef track) const
{
    if (! expanded (track))
        return 0;

    auto height = 0;

    for (const auto& lane : view.lanes (track))
        height += lane.heightPx;

    return height;
}

std::vector<AutomationLaneDrawing> AutomationLanes::lanes (duet::model::TrackRef track) const
{
    std::vector<AutomationLaneDrawing> drawings;

    if (! expanded (track))
        return drawings;

    const auto stored = view.lanes (track);
    const auto perSecond = beatsPerSecond();
    auto y = 0;
    auto index = 0;

    for (const auto& lane : stored)
    {
        const auto option = optionFor (lane.target, track);

        AutomationLaneDrawing drawing;
        drawing.track = track;
        drawing.index = index++;
        drawing.target = lane.target;
        drawing.targetName = option.name;
        drawing.minimumValue = option.minimumValue;
        drawing.maximumValue = option.maximumValue;
        drawing.skew = option.skew;
        drawing.y = y;
        drawing.height = lane.heightPx;

        if (session != nullptr)
        {
            for (const auto& point : session->automationPoints (lane.target))
            {
                AutomationPointDrawing pointDrawing;
                pointDrawing.beats = point.timeSeconds * perSecond;
                pointDrawing.value = point.value;
                pointDrawing.curvature = point.curvature;
                pointDrawing.x = timeline.beatsToX (pointDrawing.beats);
                pointDrawing.y = yForValue (drawing, pointDrawing.value);

                drawing.points.push_back (pointDrawing);
            }
        }

        y += drawing.height;
        drawings.push_back (std::move (drawing));
    }

    return drawings;
}

double
    AutomationLanes::valueAtBeats (duet::model::TrackRef track, int laneIndex, double beats) const
{
    const auto drawings = lanes (track);

    if (laneIndex < 0 || laneIndex >= static_cast<int> (drawings.size()))
        return 0.0;

    const auto& lane = drawings[static_cast<std::size_t> (laneIndex)];

    if (lane.points.empty())
        return session == nullptr
                   ? 0.0
                   : session->automationValueAt (lane.target, beats / beatsPerSecond());

    if (beats <= lane.points.front().beats)
        return lane.points.front().value;

    if (beats >= lane.points.back().beats)
        return lane.points.back().value;

    for (std::size_t point = 1; point < lane.points.size(); ++point)
    {
        const auto& before = lane.points[point - 1];
        const auto& after = lane.points[point];

        if (beats > after.beats)
            continue;

        const auto span = after.beats - before.beats;

        if (span <= 0.0)
            return after.value;

        const auto along = (beats - before.beats) / span;

        return before.value + along * (after.value - before.value);
    }

    return lane.points.back().value;
}

//==============================================================================
std::vector<AutomationTargetOption> AutomationLanes::targetsFor (duet::model::TrackRef track) const
{
    std::vector<AutomationTargetOption> options;

    if (session == nullptr || track == duet::model::noTrack)
        return options;

    // The fader's own range, so that a volume lane and the fader in the mixer
    // are the same control drawn twice.
    options.push_back ({ duet::model::AutomationTarget::trackVolumeOf (track),
                         volumeOptionName,
                         Mixer::faderMinimumDb,
                         Mixer::faderMaximumDb });
    options.push_back ({ duet::model::AutomationTarget::trackPanOf (track),
                         panOptionName,
                         panMinimum,
                         panMaximum });

    for (const auto& plugin : session->track (track).plugins)
        for (const auto& parameter : session->pluginParameters (plugin.plugin))
            options.push_back (
                { duet::model::AutomationTarget::parameterOf (plugin.plugin, parameter.parameterId),
                  plugin.name + ": " + parameter.name,
                  parameter.minValue,
                  parameter.maxValue,
                  parameter.skew });

    return options;
}

void AutomationLanes::setLaneTarget (duet::model::TrackRef track,
                                     int laneIndex,
                                     const duet::model::AutomationTarget& target)
{
    auto stored = view.lanes (track);

    if (laneIndex < 0 || laneIndex >= static_cast<int> (stored.size()))
        return;

    stored[static_cast<std::size_t> (laneIndex)].target = target;
    view.setLanes (track, std::move (stored));
}

//==============================================================================
void AutomationLanes::addPoint (duet::model::TrackRef track,
                                int laneIndex,
                                double atBeats,
                                int y,
                                bool altHeld)
{
    if (session == nullptr)
        return;

    const auto drawings = lanes (track);

    if (laneIndex < 0 || laneIndex >= static_cast<int> (drawings.size()))
        return;

    const auto& lane = drawings[static_cast<std::size_t> (laneIndex)];
    const auto beats = std::max (0.0, snapBeats (atBeats, timeline.gridFor(), altHeld));
    const auto seconds = beats / beatsPerSecond();
    const auto value = valueAtY (lane, y);

    session->performAction ("Add Automation Point",
                            [&] (auto& ops)
                            { ops.setAutomationPoints (lane.target, { { seconds, value } }); });
}

bool AutomationLanes::removePoint (duet::model::TrackRef track, int laneIndex, int x, int y)
{
    if (session == nullptr)
        return false;

    const auto drawings = lanes (track);

    if (laneIndex < 0 || laneIndex >= static_cast<int> (drawings.size()))
        return false;

    const auto& lane = drawings[static_cast<std::size_t> (laneIndex)];
    const auto point = pointIndexAt (lane, x, y);

    if (! point.has_value())
        return false;

    const auto seconds = lane.points[static_cast<std::size_t> (*point)].beats / beatsPerSecond();

    session->performAction ("Remove Automation Point",
                            [&] (auto& ops)
                            { ops.removeAutomationPoints (lane.target, seconds, seconds); });

    return true;
}

//==============================================================================
void AutomationLanes::beginPointGesture (duet::model::TrackRef track, int laneIndex, int pointIndex)
{
    gesture.reset();

    const auto drawings = lanes (track);

    if (laneIndex < 0 || laneIndex >= static_cast<int> (drawings.size()))
        return;

    const auto& lane = drawings[static_cast<std::size_t> (laneIndex)];

    if (pointIndex < 0 || pointIndex >= static_cast<int> (lane.points.size()))
        return;

    const auto& point = lane.points[static_cast<std::size_t> (pointIndex)];

    Gesture dragging;
    dragging.target = lane.target;
    dragging.originalSeconds = point.beats / beatsPerSecond();
    dragging.curvature = point.curvature;
    dragging.destinationBeats = point.beats;
    dragging.value = point.value;
    dragging.lane = lane;

    gesture = dragging;
}

void AutomationLanes::updatePointGesture (double atBeats, int y, bool altHeld)
{
    if (! gesture.has_value())
        return;

    gesture->destinationBeats = std::max (0.0, snapBeats (atBeats, timeline.gridFor(), altHeld));
    gesture->value = valueAtY (gesture->lane, y);
}

bool AutomationLanes::completePointGesture()
{
    if (! gesture.has_value() || session == nullptr)
    {
        gesture.reset();
        return false;
    }

    const auto dragged = *gesture;
    gesture.reset();

    const auto seconds = dragged.destinationBeats / beatsPerSecond();

    // Taking the point off its old time and putting it on its new one is one
    // Action, so one undo puts the whole drag back. The curvature goes with it:
    // a point that was moved is the same point.
    session->performAction (
        "Move Automation Point",
        [&] (auto& ops)
        {
            ops.removeAutomationPoints (
                dragged.target, dragged.originalSeconds, dragged.originalSeconds);
            ops.setAutomationPoints (dragged.target,
                                     { { seconds, dragged.value, dragged.curvature } });
        });

    return true;
}

void AutomationLanes::cancelPointGesture() { gesture.reset(); }

int AutomationLanes::addLane (duet::model::TrackRef track)
{
    if (track == duet::model::noTrack)
        return -1;

    auto stored = view.lanes (track);

    LaneView lane;
    lane.target = duet::model::AutomationTarget::trackVolumeOf (track);

    stored.push_back (lane);
    const auto index = static_cast<int> (stored.size()) - 1;
    view.setLanes (track, std::move (stored));

    return index;
}

void AutomationLanes::removeLane (duet::model::TrackRef track, int laneIndex)
{
    auto stored = view.lanes (track);

    if (laneIndex < 0 || laneIndex >= static_cast<int> (stored.size()))
        return;

    stored.erase (stored.begin() + laneIndex);
    view.setLanes (track, std::move (stored));
}

//==============================================================================
double AutomationLanes::beatsPerSecond() const
{
    return session == nullptr ? 2.0 : std::max (1.0, session->tempoBpm()) / 60.0;
}

AutomationTargetOption AutomationLanes::optionFor (const duet::model::AutomationTarget& target,
                                                   duet::model::TrackRef track) const
{
    for (const auto& option : targetsFor (track))
        if (sameTarget (option.target, target))
            return option;

    // A lane bound to a plugin the producer has since taken off the track: it
    // keeps its target, so that putting the plugin back brings the curve back
    // with it, and draws against the widest range it could be asked for.
    return { target, target.parameterId, 0.0, 1.0 };
}
} // namespace duet::gui
