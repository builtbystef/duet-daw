#include <duet/gui/ArrangementView.h>

#include <duet/gui/SessionClock.h>
#include <duet/gui/TimelineClock.h>
#include <duet/gui/TimelineGeometry.h>
#include <duet/gui/ViewState.h>

#include <duet/model/Session.h>
#include <duet/testing/TestSupport.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;
using duet::gui::ArrangementView;
using duet::gui::Command;
using duet::gui::ScrollGesture;
using duet::gui::ViewState;

namespace
{
/** A ref that names a track. What it is worth does not matter here. */
constexpr duet::model::TrackRef bass = 41;

/** The arrangement at a size, on a view scrolled to a known place. */
struct OpenArrangement
{
    OpenArrangement()
    {
        view.setHZoomPxPerBeat (40.0);
        view.setVScrollPx (200);
        view.setTrackHeightPx (bass, 100);
        arrangement.setWidthPx (800);
    }

    ViewState view;
    ArrangementView arrangement { view };
};

class Clock final : public duet::gui::TimelineClock
{
public:
    [[nodiscard]] double beatsPerBar() const override { return 4.0; }
    [[nodiscard]] double playheadBeats() const override { return position; }
    void setPlayheadBeats (double beats) override { position = beats; }
    [[nodiscard]] bool isPlaying() const override { return playing; }
    [[nodiscard]] double contentLengthBeats() const override { return 64.0; }

    double position = 0.0;
    bool playing = false;
};

/** One notch of a wheel, the way a mouse reports it: away from the producer. */
[[nodiscard]] ScrollGesture notches (double count)
{
    ScrollGesture gesture;

    gesture.deltaY = count;
    return gesture;
}
} // namespace

TEST_CASE ("follow playhead scrolls a rolling transport into view only while enabled")
{
    OpenArrangement open;
    Clock clock;
    open.arrangement.setClock (&clock);
    clock.position = 30.0;
    clock.playing = true;

    REQUIRE (open.arrangement.followPlayback());
    REQUIRE (open.arrangement.playheadX() > 0);
    REQUIRE (open.arrangement.playheadX() < open.arrangement.geometry().widthPx());

    open.view.setHScrollBeats (0.0);
    open.view.setFollowPlayhead (false);
    REQUIRE_FALSE (open.arrangement.followPlayback());
    REQUIRE (open.view.hScrollBeats() == 0.0);
}

TEST_CASE ("scrolling with no modifier moves the arrangement up and down")
{
    OpenArrangement open;

    open.arrangement.scroll (notches (1.0));

    REQUIRE (open.view.vScrollPx() == 200 - ArrangementView::scrollPixelsPerNotch);

    open.arrangement.scroll (notches (-1.0));

    REQUIRE (open.view.vScrollPx() == 200);

    // Nothing is above the first track.
    open.arrangement.scroll (notches (100.0));

    REQUIRE (open.view.vScrollPx() == 0);
}

TEST_CASE ("scrolling with shift moves the arrangement sideways")
{
    OpenArrangement open;

    auto gesture = notches (-1.0);
    gesture.shift = true;

    open.arrangement.scroll (gesture);

    // 60 px at 40 px/beat is a beat and a half.
    REQUIRE_THAT (open.view.hScrollBeats(), WithinAbs (1.5, 1e-9));
    REQUIRE (open.view.vScrollPx() == 200);
}

TEST_CASE ("scrolling with ctrl zooms the timeline about the pointer")
{
    OpenArrangement open;

    open.view.setHZoomPxPerBeat (100.0);
    open.view.setHScrollBeats (4.0);

    const auto underThePointer = open.arrangement.geometry().xToBeats (400);

    auto gesture = notches (1.0);
    gesture.ctrl = true;
    gesture.pointerX = 400;

    open.arrangement.scroll (gesture);

    REQUIRE (open.view.hZoomPxPerBeat() > 100.0);
    REQUIRE_THAT (open.arrangement.geometry().xToBeats (400), WithinAbs (underThePointer, 1e-9));
}

TEST_CASE ("scrolling with ctrl and shift makes the tracks taller and shorter")
{
    OpenArrangement open;

    auto gesture = notches (1.0);
    gesture.ctrl = true;
    gesture.shift = true;
    gesture.pointerX = 400;

    const auto zoomBefore = open.view.hZoomPxPerBeat();

    open.arrangement.scroll (gesture);

    REQUIRE (open.view.trackHeightPx (bass) > 100);
    REQUIRE_THAT (open.view.hZoomPxPerBeat(), WithinAbs (zoomBefore, 1e-9));

    open.arrangement.scroll (gesture);
    open.arrangement.scroll (notches (-1.0));
    REQUIRE (open.view.trackHeightPx (bass) > 100);
}

TEST_CASE ("the zoom keys zoom about the middle of the canvas")
{
    OpenArrangement open;

    const auto middle = open.arrangement.geometry().xToBeats (400);

    open.arrangement.perform (Command::zoomIn);

    REQUIRE (open.view.hZoomPxPerBeat() > 40.0);
    REQUIRE_THAT (open.arrangement.geometry().xToBeats (400), WithinAbs (middle, 1e-9));

    open.arrangement.perform (Command::zoomOut);

    REQUIRE_THAT (open.view.hZoomPxPerBeat(), WithinAbs (40.0, 1e-9));
}

TEST_CASE ("the playhead is drawn where the transport is")
{
    const duet::testing::TempProject temp;
    duet::model::Session session { temp.editFile() };
    duet::gui::SessionClock clock { session };

    session.performAction ("Set Tempo", [] (auto& ops) { ops.setTempo (120.0); });

    OpenArrangement open;
    open.arrangement.setClock (&clock);

    // Two seconds at 120 BPM is beat 4, and beat 4 at 40 px/beat is 160 px in.
    session.setPlaybackPositionSeconds (2.0);

    REQUIRE (open.arrangement.playheadX() == 160);
    REQUIRE_FALSE (open.arrangement.isPlaying());

    SECTION ("and it follows the transport as it rolls")
    {
        session.setPlaybackPositionSeconds (0.0);
        session.loadDemoContent();

        // A position that moves needs a device to move against: the engine
        // publishes it from the message thread, and a graph fed by hand leaves
        // it standing still (engine notes, further facts).
        if (session.audioDeviceDescription().empty())
            SKIP ("this machine has no audio device to play through");

        REQUIRE (duet::testing::playUntilRolling (session));
        REQUIRE (open.arrangement.isPlaying());

        // The transport rolls before the position it publishes has moved, so
        // the loop is what waits for the playhead rather than a sleep long
        // enough to have waited for it.
        for (int attempt = 0; attempt < 20 && session.playbackPositionSeconds() <= 0.0; ++attempt)
            duet::testing::pumpMessages (50);

        REQUIRE (session.playbackPositionSeconds() > 0.0);
        REQUIRE (open.arrangement.playheadX() > 0);

        session.stopPlayback();
    }
}

TEST_CASE ("clicking the ruler moves the playhead and is not something an undo puts back")
{
    const duet::testing::TempProject temp;
    duet::model::Session session { temp.editFile() };
    duet::gui::SessionClock clock { session };

    // One Action, so that the producer's history has something in it to compare
    // the click with. The tempo is the 120 BPM the worked examples are written
    // at, which is what a project starts at.
    session.performAction (
        "Add Track", [] (auto& ops) { ops.createTrack (duet::model::TrackKind::audio, "Bass"); });

    OpenArrangement open;
    open.arrangement.setClock (&clock);
    open.arrangement.clickRuler (160);

    REQUIRE_THAT (session.playbackPositionSeconds(), WithinAbs (2.0, 1e-6));

    // The producer's history holds the tempo they set, and nothing about where
    // they are listening from (ADR 0004).
    REQUIRE (session.undoNames() == std::vector<std::string> { "Add Track" });

    // A click left of the start of the project is the start of the project.
    open.arrangement.clickRuler (-400);

    REQUIRE_THAT (session.playbackPositionSeconds(), WithinAbs (0.0, 1e-6));
}

TEST_CASE ("zoom to fit fits what the project holds")
{
    const duet::testing::TempProject temp;
    duet::model::Session session { temp.editFile() };
    duet::gui::SessionClock clock { session };

    session.performAction ("Set Tempo", [] (auto& ops) { ops.setTempo (120.0); });
    session.loadDemoContent();

    OpenArrangement open;
    open.arrangement.setClock (&clock);
    open.arrangement.perform (Command::zoomToFit);

    const auto contentBeats = session.editLengthSeconds() * 2.0;

    REQUIRE (contentBeats > 0.0);
    REQUIRE_THAT (open.view.hScrollBeats(), WithinAbs (0.0, 1e-9));
    REQUIRE (open.arrangement.geometry().beatsToX (contentBeats) == 800);
}

TEST_CASE ("the grid counts the bars the project's time signature says")
{
    const duet::testing::TempProject temp;
    duet::model::Session session { temp.editFile() };
    duet::gui::SessionClock clock { session };

    session.performAction ("Set Time Signature", [] (auto& ops) { ops.setTimeSignature (3, 4); });

    OpenArrangement open;
    open.arrangement.setClock (&clock);

    REQUIRE_THAT (open.arrangement.geometry().gridFor().barBeats, WithinAbs (3.0, 1e-9));

    // 6/8 is six eighth notes, which is three of the quarter-note beats the
    // timeline counts in.
    session.performAction ("Set Time Signature", [] (auto& ops) { ops.setTimeSignature (6, 8); });
    open.arrangement.refresh();

    REQUIRE_THAT (open.arrangement.geometry().gridFor().barBeats, WithinAbs (3.0, 1e-9));
}
