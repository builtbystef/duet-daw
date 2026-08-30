#include "Vst3FixtureHarness.h"

#include <duet/gui/AutomationLanes.h>

#include <duet/gui/ArrangementView.h>
#include <duet/gui/ViewState.h>

#include <duet/model/Session.h>
#include <duet/persistence/Project.h>
#include <duet/testing/RenderHarness.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;
using duet::gui::ArrangementView;
using duet::gui::AutomationLanes;
using duet::gui::ViewState;
using duet::model::AutomationTarget;
using duet::model::PluginRef;
using duet::model::Session;
using duet::model::TrackKind;
using duet::model::TrackRef;
using duet::testing::TempProject;

namespace
{
/** An open project with one track, seen through the arrangement the lanes hang
    under: the grid, the zoom and the scroll are the arrangement's, which is what
    makes a lane's geometry the same geometry the clips above it are drawn on.
*/
struct OpenAutomation
{
    OpenAutomation() : session (project.editFile())
    {
        arrangement.setSession (&session);
        arrangement.setWidthPx (800);
        arrangement.setHeightPx (400);

        // Twenty pixels to a beat: beat lines are the finest grid the zoom has
        // room for, so the snap is to whole beats.
        view.setHZoomPxPerBeat (20.0);

        session.performAction (
            "Add a track", [&] (auto& ops) { track = ops.createTrack (TrackKind::audio, "Keys"); });
    }

    [[nodiscard]] AutomationLanes& automation() { return arrangement.automation(); }

    TempProject project;
    Session session;
    ViewState view;
    ArrangementView arrangement { view };
    TrackRef track = duet::model::noTrack;
};

/** One track's row, found by the track it belongs to. */
[[nodiscard]] duet::gui::TrackDrawing rowFor (ArrangementView& arrangement, TrackRef track)
{
    for (const auto& row : arrangement.tracks())
        if (row.track == track)
            return row;

    return {};
}

/** One of a plugin's parameters, found by what its lane would be called. A test
    that needs a parameter of a given shape asks for it by name rather than by
    an id the engine chose.
*/
[[nodiscard]] AutomationTarget targetNamed (const AutomationLanes& lanes,
                                            TrackRef track,
                                            PluginRef plugin,
                                            const std::string& parameterId)
{
    for (const auto& option : lanes.targetsFor (track))
        if (option.target.plugin == plugin && option.target.parameterId == parameterId)
            return option.target;

    return {};
}

/** The id of the first parameter of a plugin measured in the given unit, and
    empty where the plugin has none. Criterion two of `3cs2ma` names a frequency
    in hertz, so a test asks for one by its unit rather than by an id.
*/
[[nodiscard]] std::string
    parameterMeasuredIn (Session& session, PluginRef plugin, const std::string& unit)
{
    for (const auto& parameter : session.pluginParameters (plugin))
        if (parameter.unit == unit)
            return parameter.parameterId;

    return {};
}

[[nodiscard]] std::vector<std::string> optionNames (const AutomationLanes& lanes, TrackRef track)
{
    std::vector<std::string> names;

    for (const auto& option : lanes.targetsFor (track))
        names.push_back (option.name);

    return names;
}
} // namespace

TEST_CASE ("a track's automation area opens on one lane and closes again")
{
    OpenAutomation open;

    REQUIRE_FALSE (open.automation().expanded (open.track));
    REQUIRE (open.automation().lanes (open.track).empty());
    REQUIRE (open.automation().heightPx (open.track) == 0);

    const auto beforeOpening = open.session.stateDigest();
    const auto undoDepth = open.session.undoNames().size();

    open.automation().setExpanded (open.track, true);

    const auto lanes = open.automation().lanes (open.track);

    REQUIRE (open.automation().expanded (open.track));

    // Opening an area is looking, not editing: nothing to undo, and nothing
    // about the project has changed.
    REQUIRE (open.session.undoNames().size() == undoDepth);
    REQUIRE (open.session.stateDigest() == beforeOpening);
    REQUIRE (lanes.size() == 1);
    REQUIRE (lanes.front().target.kind == AutomationTarget::Kind::trackVolume);
    REQUIRE (lanes.front().target.track == open.track);
    REQUIRE (open.automation().heightPx (open.track) == lanes.front().height);

    SECTION ("closing it leaves the lanes where they were")
    {
        open.automation().setExpanded (open.track, false);

        REQUIRE_FALSE (open.automation().expanded (open.track));
        REQUIRE (open.automation().lanes (open.track).empty());
        REQUIRE (open.automation().heightPx (open.track) == 0);

        open.automation().setExpanded (open.track, true);
        REQUIRE (open.automation().lanes (open.track).size() == 1);
    }
}

TEST_CASE ("a lane's picker offers the track's own fader and every parameter of its plugins")
{
    OpenAutomation open;
    PluginRef compressor = duet::model::noPlugin;

    open.session.performAction (
        "Compress it",
        [&] (auto& ops)
        { compressor = ops.addPlugin (open.track, duet::model::BuiltinPlugin::compressor, 0); });

    const auto options = open.automation().targetsFor (open.track);
    const auto names = optionNames (open.automation(), open.track);

    REQUIRE (options.size() >= 3);
    REQUIRE (options[0].target.kind == AutomationTarget::Kind::trackVolume);
    REQUIRE (options[1].target.kind == AutomationTarget::Kind::trackPan);
    REQUIRE (names[0] == "Volume");
    REQUIRE (names[1] == "Pan");

    const auto ratio = std::find_if (options.begin(),
                                     options.end(),
                                     [compressor] (const auto& option) {
                                         return option.target.plugin == compressor
                                                && option.target.parameterId == "ratio";
                                     });

    REQUIRE (ratio != options.end());
    REQUIRE (ratio->target.kind == AutomationTarget::Kind::pluginParameter);

    SECTION ("switching a lane's target redraws it against that parameter's own range")
    {
        open.automation().setExpanded (open.track, true);

        const auto volumeLane = open.automation().lanes (open.track).front();

        REQUIRE_THAT (volumeLane.minimumValue, WithinAbs (-60.0, 0.001));
        REQUIRE_THAT (volumeLane.maximumValue, WithinAbs (6.0, 0.001));

        open.automation().setLaneTarget (open.track, 0, AutomationTarget::trackPanOf (open.track));

        const auto panLane = open.automation().lanes (open.track).front();

        REQUIRE (panLane.target.kind == AutomationTarget::Kind::trackPan);
        REQUIRE_THAT (panLane.minimumValue, WithinAbs (-1.0, 0.001));
        REQUIRE_THAT (panLane.maximumValue, WithinAbs (1.0, 0.001));

        open.automation().setLaneTarget (open.track, 0, ratio->target);

        const auto ratioLane = open.automation().lanes (open.track).front();

        REQUIRE (ratioLane.targetName == ratio->name);
        REQUIRE_THAT (ratioLane.minimumValue, WithinAbs (ratio->minimumValue, 0.001));
        REQUIRE_THAT (ratioLane.maximumValue, WithinAbs (ratio->maximumValue, 0.001));
    }
}

TEST_CASE ("a compressor ratio lane draws the ratios a producer uses across its height")
{
    OpenAutomation open;
    PluginRef compressor = duet::model::noPlugin;

    open.session.performAction (
        "Compress it",
        [&] (auto& ops)
        { compressor = ops.addPlugin (open.track, duet::model::BuiltinPlugin::compressor, 0); });

    open.automation().setExpanded (open.track, true);
    open.automation().setLaneTarget (
        open.track, 0, targetNamed (open.automation(), open.track, compressor, "ratio"));

    const auto lane = open.automation().lanes (open.track).front();

    REQUIRE (lane.target.kind == AutomationTarget::Kind::pluginParameter);
    REQUIRE (lane.target.parameterId == "ratio");

    // The range is the engine's own, and it has to be: 1000 to one is what its
    // compressor does at the bottom of its scale.
    REQUIRE_THAT (lane.minimumValue, WithinAbs (1.0 / 0.95, 0.001));
    REQUIRE_THAT (lane.maximumValue, WithinAbs (1000.0, 0.001));

    // Four to one is the ratio a producer reaches for first, and it draws at
    // the middle of the lane. Measured at 3cs2ma: the skew that centres it is
    // 0.11898, and the 0.12 the table carries puts it at 0.497 of the height.
    REQUIRE (duet::gui::yForValue (lane, 4.0) == lane.y + lane.height / 2);

    // Two to one through twenty to one is the span a producer works in. Drawn
    // linearly all of it landed within one pixel of the floor; it now has a
    // stretch of the lane wide enough to aim inside.
    const auto atTwo = duet::gui::yForValue (lane, 2.0);
    const auto atTwenty = duet::gui::yForValue (lane, 20.0);

    REQUIRE (atTwo > duet::gui::yForValue (lane, 4.0));
    REQUIRE (atTwenty < duet::gui::yForValue (lane, 4.0));
    REQUIRE (atTwo - atTwenty >= lane.height / 8);

    SECTION ("a drag up the lane walks through those ratios rather than past them")
    {
        // What the heights either side of the middle stand for: a drag has to
        // be able to land on the useful ratios, not step over them.
        REQUIRE (duet::gui::valueAtY (lane, lane.y + lane.height / 2) > 3.0);
        REQUIRE (duet::gui::valueAtY (lane, lane.y + lane.height / 2) < 5.0);

        // A quarter of the way down from the top is still a ratio with a name,
        // not the far end of the scale.
        REQUIRE (duet::gui::valueAtY (lane, lane.y + lane.height / 4) < 200.0);

        // Every height the lane has maps back to where it was read from, so a
        // point drawn at a value is grabbed at the same place it was drawn.
        for (const double ratio : { 1.5, 2.0, 4.0, 8.0, 20.0, 100.0 })
        {
            const auto y = duet::gui::yForValue (lane, ratio);

            REQUIRE (std::abs (duet::gui::yForValue (lane, duet::gui::valueAtY (lane, y)) - y)
                     <= 1);
        }
    }
}

TEST_CASE ("a lane whose range is already linear in the producer's terms draws evenly")
{
    OpenAutomation open;
    PluginRef equaliser = duet::model::noPlugin;

    open.session.performAction (
        "Shape it",
        [&] (auto& ops)
        { equaliser = ops.addPlugin (open.track, duet::model::BuiltinPlugin::eq, 0); });

    open.automation().setExpanded (open.track, true);

    const auto frequency = parameterMeasuredIn (open.session, equaliser, "Hz");

    REQUIRE_FALSE (frequency.empty());

    open.automation().setLaneTarget (
        open.track, 0, targetNamed (open.automation(), open.track, equaliser, frequency));

    const auto lane = open.automation().lanes (open.track).front();

    // The lane really is bound to that parameter, and not to the 0 to 1 a lane
    // falls back to when it cannot find its target.
    REQUIRE (lane.target.kind == AutomationTarget::Kind::pluginParameter);
    REQUIRE (lane.target.parameterId == frequency);
    REQUIRE (lane.maximumValue > lane.minimumValue);

    // Hertz on the engine's equaliser are the producer's own number, so the
    // lane is the plain proportion it has always been: the value a quarter of
    // the way down is a quarter of the way down the range, and the middle of
    // the range is the middle of the lane.
    const auto span = lane.maximumValue - lane.minimumValue;

    REQUIRE_THAT (duet::gui::valueAtY (lane, lane.y + lane.height / 4),
                  WithinAbs (lane.minimumValue + 0.75 * span, 1e-9));
    REQUIRE_THAT (duet::gui::valueAtY (lane, lane.y + lane.height / 2),
                  WithinAbs (lane.minimumValue + 0.5 * span, 1e-9));

    REQUIRE (duet::gui::yForValue (lane, lane.minimumValue + 0.5 * span)
             == lane.y + lane.height / 2);
    REQUIRE (duet::gui::yForValue (lane, lane.maximumValue) == lane.y);
}

TEST_CASE ("adding a point puts it on the grid at the value the height stands for")
{
    OpenAutomation open;

    open.automation().setExpanded (open.track, true);
    open.automation().setLaneTarget (open.track, 0, AutomationTarget::trackPanOf (open.track));

    const auto lane = open.automation().lanes (open.track).front();
    const auto beforeThePoint = open.session.stateDigest();

    // The worked example of issue 3rd6lu: grid = 1 beat, a double-click at 2.4
    // beats three quarters of the way down a lane. The lane runs from +1 at its
    // top to −1 at its bottom, so three quarters down is −0.5.
    const auto threeQuartersDown = lane.y + (lane.height * 3) / 4;

    REQUIRE_THAT (duet::gui::valueAtY (lane, threeQuartersDown), WithinAbs (-0.5, 1e-9));

    open.automation().addPoint (open.track, 0, 2.4, threeQuartersDown, false);

    const auto points = open.automation().lanes (open.track).front().points;

    REQUIRE (points.size() == 1);
    REQUIRE_THAT (points.front().beats, WithinAbs (2.0, 1e-9));
    REQUIRE_THAT (points.front().value, WithinAbs (-0.5, 1e-6));
    REQUIRE (points.front().x == open.arrangement.geometry().beatsToX (2.0));
    REQUIRE (points.front().y == threeQuartersDown);

    SECTION ("it is exactly one Action, and undo takes the project back digest-exactly")
    {
        REQUIRE (open.session.undoNames().front() == "Add Automation Point");
        REQUIRE (open.session.undo());
        REQUIRE (open.session.stateDigest() == beforeThePoint);
        REQUIRE (open.automation().lanes (open.track).front().points.empty());
    }

    SECTION ("Alt lands the point where the pointer was")
    {
        open.automation().addPoint (open.track, 0, 5.4, threeQuartersDown, true);

        const auto unsnapped = open.automation().lanes (open.track).front().points;

        REQUIRE (unsnapped.size() == 2);
        REQUIRE_THAT (unsnapped.back().beats, WithinAbs (5.4, 1e-9));
    }
}

TEST_CASE ("dragging a point snaps sideways, moves freely up and down, and is one Action")
{
    OpenAutomation open;

    open.automation().setExpanded (open.track, true);
    open.automation().setLaneTarget (open.track, 0, AutomationTarget::trackPanOf (open.track));

    auto lane = open.automation().lanes (open.track).front();
    const auto quarterDown = lane.y + lane.height / 4;

    open.automation().addPoint (open.track, 0, 2.0, lane.y + (lane.height * 3) / 4, false);

    const auto beforeTheDrag = open.session.stateDigest();

    open.automation().beginPointGesture (open.track, 0, 0);
    open.automation().updatePointGesture (5.3, quarterDown, false);

    REQUIRE (open.automation().hasPointGesture());
    REQUIRE (open.automation().completePointGesture());

    const auto moved = open.automation().lanes (open.track).front().points;

    REQUIRE (moved.size() == 1);
    REQUIRE_THAT (moved.front().beats, WithinAbs (5.0, 1e-9));
    REQUIRE_THAT (moved.front().value, WithinAbs (0.5, 1e-6));
    REQUIRE (open.session.undoNames().front() == "Move Automation Point");

    SECTION ("one undo takes the whole drag back")
    {
        REQUIRE (open.session.undo());
        REQUIRE (open.session.stateDigest() == beforeTheDrag);
    }
}

TEST_CASE ("Alt bypasses a drag's horizontal snap, and Escape abandons the drag entirely")
{
    OpenAutomation open;

    open.automation().setExpanded (open.track, true);
    open.automation().setLaneTarget (open.track, 0, AutomationTarget::trackPanOf (open.track));

    const auto lane = open.automation().lanes (open.track).front();

    open.automation().addPoint (open.track, 0, 2.0, lane.y + (lane.height * 3) / 4, false);

    const auto afterThePoint = open.session.stateDigest();
    const auto undoDepth = open.session.undoNames().size();

    SECTION ("Alt held through the drag lands the point where the pointer was")
    {
        open.automation().beginPointGesture (open.track, 0, 0);
        open.automation().updatePointGesture (5.3, lane.y + lane.height / 4, true);

        REQUIRE (open.automation().completePointGesture());
        REQUIRE_THAT (open.automation().lanes (open.track).front().points.front().beats,
                      WithinAbs (5.3, 1e-9));
    }

    SECTION ("an abandoned drag emits no Action at all")
    {
        open.automation().beginPointGesture (open.track, 0, 0);
        open.automation().updatePointGesture (5.3, lane.y + lane.height / 4, false);
        open.automation().cancelPointGesture();

        REQUIRE_FALSE (open.automation().hasPointGesture());
        REQUIRE (open.session.stateDigest() == afterThePoint);
        REQUIRE (open.session.undoNames().size() == undoDepth);
    }
}

TEST_CASE ("a drag carries a point's curvature through untouched")
{
    OpenAutomation open;

    open.automation().setExpanded (open.track, true);
    open.automation().setLaneTarget (open.track, 0, AutomationTarget::trackPanOf (open.track));

    const auto pan = AutomationTarget::trackPanOf (open.track);

    // A curved segment, as a later milestone will write one. Moving its point
    // must not quietly straighten it.
    open.session.performAction ("Bend a segment",
                                [&] (auto& ops)
                                { ops.setAutomationPoints (pan, { { 1.0, 0.25, 0.5 } }); });

    const auto lane = open.automation().lanes (open.track).front();

    REQUIRE_THAT (lane.points.front().curvature, WithinAbs (0.5, 0.001));

    open.automation().beginPointGesture (open.track, 0, 0);
    open.automation().updatePointGesture (4.0, lane.y + lane.height / 4, false);

    REQUIRE (open.automation().completePointGesture());

    const auto moved = open.automation().lanes (open.track).front().points;

    REQUIRE (moved.size() == 1);
    REQUIRE_THAT (moved.front().beats, WithinAbs (4.0, 1e-9));
    REQUIRE_THAT (moved.front().curvature, WithinAbs (0.5, 0.001));
}

TEST_CASE ("right-clicking a point removes it, and empty lane space is not a point")
{
    OpenAutomation open;

    open.automation().setExpanded (open.track, true);
    open.automation().setLaneTarget (open.track, 0, AutomationTarget::trackPanOf (open.track));

    const auto lane = open.automation().lanes (open.track).front();
    const auto pointY = lane.y + (lane.height * 3) / 4;

    open.automation().addPoint (open.track, 0, 2.0, pointY, false);

    const auto withThePoint = open.session.stateDigest();
    const auto placed = open.automation().lanes (open.track).front().points.front();

    SECTION ("a click nowhere near a point is nothing at all")
    {
        const auto empty = open.automation().lanes (open.track).front();

        REQUIRE_FALSE (duet::gui::pointIndexAt (empty, placed.x + 60, placed.y).has_value());
        REQUIRE_FALSE (open.automation().removePoint (open.track, 0, placed.x + 60, placed.y));
        REQUIRE (open.session.stateDigest() == withThePoint);
        REQUIRE (open.automation().lanes (open.track).front().points.size() == 1);
    }

    SECTION ("a click on the point takes it off, as one Action")
    {
        REQUIRE (open.automation().removePoint (open.track, 0, placed.x, placed.y));
        REQUIRE (open.automation().lanes (open.track).front().points.empty());
        REQUIRE (open.session.undoNames().front() == "Remove Automation Point");

        REQUIRE (open.session.undo());
        REQUIRE (open.session.stateDigest() == withThePoint);
    }
}

TEST_CASE ("a lane draws straight segments, and the engine plays what it draws")
{
    OpenAutomation open;

    open.automation().setExpanded (open.track, true);
    open.automation().setLaneTarget (open.track, 0, AutomationTarget::trackPanOf (open.track));

    const auto pan = AutomationTarget::trackPanOf (open.track);

    // The worked example of issue 3rd6lu: beat 0.0 at 0.0 and beat 4.0 at 1.0.
    // Four beats at 120 BPM is two seconds.
    open.session.performAction ("Sweep the pan",
                                [&] (auto& ops)
                                { ops.setAutomationPoints (pan, { { 0.0, 0.0 }, { 2.0, 1.0 } }); });

    const auto lane = open.automation().lanes (open.track).front();

    REQUIRE (lane.points.size() == 2);
    REQUIRE_THAT (lane.points.front().beats, WithinAbs (0.0, 1e-9));
    REQUIRE_THAT (lane.points.back().beats, WithinAbs (4.0, 1e-9));

    REQUIRE_THAT (open.automation().valueAtBeats (open.track, 0, 2.0), WithinAbs (0.5, 1e-9));
    REQUIRE_THAT (open.automation().valueAtBeats (open.track, 0, 1.0), WithinAbs (0.25, 1e-9));

    // What the lane draws is what the engine plays, at the same beat.
    REQUIRE_THAT (open.session.automationValueAt (pan, 1.0),
                  WithinAbs (open.automation().valueAtBeats (open.track, 0, 2.0), 0.001));

    SECTION ("a curve holds its end values beyond its last point")
    {
        REQUIRE_THAT (open.automation().valueAtBeats (open.track, 0, 12.0), WithinAbs (1.0, 1e-9));
    }
}

TEST_CASE ("a lane, its target, and the value of every point come back after a save and reopen")
{
    const TempProject temp;
    const auto folder = temp.folder() / "Nocturne";
    TrackRef track = duet::model::noTrack;

    {
        const auto project = duet::persistence::Project::create (folder);
        REQUIRE (project != nullptr);

        ViewState view;
        ArrangementView arrangement { view };

        project->onCaptureViewState ([&view] { return view.toData(); });
        arrangement.setSession (&project->session());
        arrangement.setWidthPx (800);
        view.setHZoomPxPerBeat (20.0);

        track = project->session().tracks().front().track;

        arrangement.automation().setExpanded (track, true);
        arrangement.automation().setLaneTarget (track, 0, AutomationTarget::trackPanOf (track));

        const auto lane = arrangement.automation().lanes (track).front();

        // An eighth of the way down a lane that runs from +1 to −1: three
        // quarters, exactly, and a value a file can hold without rounding.
        arrangement.automation().addPoint (track, 0, 2.0, lane.y + lane.height / 8, false);

        REQUIRE (arrangement.automation().lanes (track).front().points.front().value == 0.75);
        REQUIRE (project->save());
    }

    const auto reopened = duet::persistence::Project::open (folder);
    REQUIRE (reopened != nullptr);

    ViewState restored;
    restored.readFrom (reopened->viewState());

    ArrangementView arrangement { restored };
    arrangement.setSession (&reopened->session());
    arrangement.setWidthPx (800);

    REQUIRE (arrangement.automation().expanded (track));

    const auto lanes = arrangement.automation().lanes (track);

    REQUIRE (lanes.size() == 1);
    REQUIRE (lanes.front().target.kind == AutomationTarget::Kind::trackPan);
    REQUIRE (lanes.front().points.size() == 1);
    REQUIRE (lanes.front().points.front().value == 0.75);
    REQUIRE_THAT (lanes.front().points.front().beats, WithinAbs (2.0, 1e-9));

    // Nothing the producer did to the view is on the undo stack: reopening a
    // project with lanes open leaves nothing to undo.
    REQUIRE (reopened->session().undoNames().empty());
}

TEST_CASE ("a curve drawn in a lane moves the fader audibly")
{
    const TempProject project;
    const auto tone = project.writeTone ("tone.wav", 2.0, 440.0);

    Session session { project.editFile() };
    session.useNoAudioDevice();
    session.suppressDeviceRebuild();

    ViewState view;
    ArrangementView arrangement { view };

    arrangement.setSession (&session);
    arrangement.setWidthPx (800);
    view.setHZoomPxPerBeat (20.0);

    TrackRef track = duet::model::noTrack;

    session.performAction ("A steady tone",
                           [&] (auto& ops)
                           {
                               track = ops.createTrack (TrackKind::audio, "Tone");
                               ops.insertAudioClip (track, "tone", tone, 0.0, 2.0);
                           });

    arrangement.automation().setExpanded (track, true);

    const auto lane = arrangement.automation().lanes (track).front();

    REQUIRE (lane.target.kind == AutomationTarget::Kind::trackVolume);

    // A step down halfway through the tone, drawn point by point: two seconds
    // is four beats at 120 BPM, and the point just before the step is off the
    // grid, which is what Alt is for.
    const auto loudY = lane.y + lane.height / 16;
    const auto quietY = lane.y + lane.height / 4;

    arrangement.automation().addPoint (track, 0, 0.0, loudY, false);
    arrangement.automation().addPoint (track, 0, 1.98, loudY, true);
    arrangement.automation().addPoint (track, 0, 2.0, quietY, false);
    arrangement.automation().addPoint (track, 0, 4.0, quietY, false);

    const auto drawn = arrangement.automation().lanes (track).front().points;

    REQUIRE (drawn.size() == 4);

    const auto drop = drawn.back().value - drawn.front().value;

    REQUIRE (drop < -3.0);

    const auto render = duet::testing::renderProject (session, project.folder());

    REQUIRE (render.readable());
    INFO ("drawn drop " << drop << " dB, measured " << render.levelChangeDb (0.1, 0.9, 1.1, 1.9)
                        << " dB");
    REQUIRE (render.levelChangeDb (0.1, 0.9, 1.1, 1.9) == Catch::Approx (drop).margin (0.5));

    // Playing the project is what hands the fader to its curve: from here on it
    // is somewhere the producer never put it.
    REQUIRE (session.liveTrackVolumeDb (track) != session.track (track).volumeDb);
}

TEST_CASE ("an open automation area takes room under its track's row")
{
    OpenAutomation open;
    TrackRef second = duet::model::noTrack;

    open.session.performAction (
        "Add another", [&] (auto& ops) { second = ops.createTrack (TrackKind::audio, "Bass"); });

    const auto closedRow = rowFor (open.arrangement, open.track);
    const auto closedBelow = rowFor (open.arrangement, second);

    REQUIRE (closedRow.automationHeight == 0);
    REQUIRE (closedBelow.y == closedRow.y + closedRow.height);

    const auto contentClosed = open.arrangement.contentHeightPx();

    open.automation().setExpanded (open.track, true);

    const auto areaHeight = open.automation().heightPx (open.track);
    const auto openedRow = rowFor (open.arrangement, open.track);
    const auto openedBelow = rowFor (open.arrangement, second);

    REQUIRE (areaHeight > 0);
    REQUIRE (openedRow.automationY == openedRow.y + openedRow.height);
    REQUIRE (openedRow.automationHeight == areaHeight);
    REQUIRE (openedBelow.y == openedRow.y + openedRow.height + areaHeight);
    REQUIRE (open.arrangement.contentHeightPx() == contentClosed + areaHeight);
}

TEST_CASE ("deleting a track takes its lanes and its open automation area with it")
{
    OpenAutomation open;

    open.automation().setExpanded (open.track, true);
    open.automation().setLaneTarget (open.track, 0, AutomationTarget::trackPanOf (open.track));

    REQUIRE (open.view.lanes (open.track).size() == 1);

    open.arrangement.deleteTrack (open.track);

    REQUIRE (open.view.lanes (open.track).empty());
    REQUIRE_FALSE (open.view.lanesExpanded (open.track));
    REQUIRE (open.automation().heightPx (open.track) == 0);
}

TEST_CASE ("a lane's picker skips a plugin that will not say what parameters it has")
{
    OpenAutomation open;

    const auto bundle =
        duet::testing::copyVst3Fixture (DUET_RAISING_VST3_FIXTURE, open.project.folder() / "vst3");
    const auto fixture = duet::testing::scanVst3Fixture (
        open.session, bundle.parent_path(), duet::testing::raisingVst3FixtureName);

    PluginRef hosted = duet::model::noPlugin;
    PluginRef compressor = duet::model::noPlugin;

    open.session.performAction ("Build the chain",
                                [&] (auto& ops)
                                {
                                    hosted = ops.addPlugin (open.track, fixture.identifier, 0);
                                    compressor = ops.addPlugin (
                                        open.track, duet::model::BuiltinPlugin::compressor, 1);
                                });

    const auto offeredFor = [&] (PluginRef plugin)
    {
        const auto options = open.automation().targetsFor (open.track);

        return std::count_if (options.begin(),
                              options.end(),
                              [plugin] (const auto& option)
                              { return option.target.plugin == plugin; });
    };

    // Both plugins offer their parameters while both are answering.
    REQUIRE (offeredFor (hosted) > 0);
    REQUIRE (offeredFor (compressor) > 0);

    duet::testing::raiseWhenRead (bundle);

    // A producer who never asks the Collaborator anything opens this list from
    // a track header, so a plugin free to raise when it is asked what a value
    // means is a menu that takes the DAW down with it. The list is built and
    // the plugin simply offers nothing to draw.
    REQUIRE_NOTHROW (open.automation().targetsFor (open.track));

    REQUIRE (offeredFor (hosted) == 0);

    // And the track keeps everything that is not that plugin's: its own two
    // curves, and the parameters of the plugin beside it in the chain.
    const auto names = optionNames (open.automation(), open.track);

    REQUIRE (names[0] == "Volume");
    REQUIRE (names[1] == "Pan");
    REQUIRE (offeredFor (compressor) > 0);
}
