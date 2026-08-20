---
id: 5he6vd
title: 'Arrangement canvas: geometry, adaptive grid, zoom, scroll, playhead'
state: done
assignee: claude
priority: high
depends_on:
    - fcsez4
    - 4r7nlj
parent: 535bbo
created: 2026-08-12T03:49:06Z
updated: 2026-08-20T10:55:21Z
---

## What to build

The arrangement's coordinate system and its ruler, the first real use of the view-model seam. A geometry view-model owns the time↔pixel mapping, the adaptive grid, and pointer-anchored zoom; it is pure, has no components in it, and later surfaces reuse it. The ruler above the arrangement draws bar, beat, and fine lines at the token weights, labelled in bars and beats. The playhead draws over the arrangement and moves with the engine transport, repainting from what the engine publishes rather than from any lock.

The contract shape (names may be refined in place, the shape is the decision):

```cpp
class TimelineGeometry {
public:
    double xToBeats (int px) const;
    int    beatsToX (double beats) const;
    // Finest subdivision whose spacing is >= 18 px at the current zoom.
    GridSpec gridFor() const;
    // Zoom anchored at a pointer: the beat under anchorPx is invariant.
    void zoomAt (int anchorPx, double factor);
};
```

Scrolling and zooming follow the conventions the whole app shares: plain scroll is vertical, Shift is horizontal, Ctrl zooms horizontally anchored at the pointer, Ctrl+Shift zooms vertically. Horizontal zoom and both scroll positions persist in the project's VIEW state.

## Acceptance criteria

- [ ] Round-trip, worked: at 40 px/beat with the view scrolled to beat 8.0, `beatsToX(10.0)` is 80 px from the left edge and `xToBeats(80)` returns 10.0.
- [ ] Adaptive grid, worked: at 20 px/beat the grid is beats (beat lines 20 px ≥ 18; quarter-beat lines would be 5 px). At 80 px/beat the grid is 1/16 notes (quarter-beats 20 px ≥ 18). The chosen subdivision is always the finest whose spacing is at least 18 px.
- [ ] Zoom anchor, worked: with beat 8.0 under x=400, `zoomAt(400, f)` for f = 2.0 and f = 0.5 both leave beat 8.0 under x=400.
- [ ] Bar, beat, and fine grid lines draw at their distinct token weights, and the ruler labels bars and beats in tabular numerals; at 4/4 120 BPM bar 3 is labelled at beat 8.0.
- [ ] Plain scroll moves vertically, Shift+scroll horizontally, Ctrl+scroll zooms horizontally about the pointer, Ctrl+Shift+scroll zooms vertically; `+` and `-` zoom horizontally about the view center and `0` fits the project's content to the visible width.
- [ ] Scrolling never goes left of beat 0, and zoom clamps at bounds that keep both the finest and coarsest useful grids reachable.
- [ ] The playhead tracks the transport during playback and lands at the clicked position when the ruler is clicked; the click is not an undoable Action.
- [ ] View round-trip: zoom to 63.5 px/beat, scroll to beat 12.25, save and reopen → both return exactly, and neither dirtied the document.
- [ ] Playhead repaint reads engine-published values only — no lock is taken in paint.

## Notes

**claude** — 2026-08-20T10:55:21Z

## What was built

Two paintless view-models in `duet::gui`, one seam, and the surface that paints them.

- `TimelineGeometry` — the time↔pixel mapping, the adaptive grid, pointer-anchored zoom, zoom-to-fit, and the lines and labels the ruler draws. It keeps no zoom or scroll of its own: it reads and writes `ViewState`, which is the one place they live and what a save captures, so the VIEW round-trip needed no new persistence work.
- `ArrangementView` — the surface over it: the scroll and zoom conventions every surface shares, the playhead, and the ruler click.
- `TimelineClock` — what a surface drawing musical time asks the open project: metre, playhead, whether it is rolling, how far the content reaches. `SessionClock` is that seam over `duet::model::Session`, and `duet_app` hands one to the shell as it opens a project and takes it back before the project goes.
- `ArrangementCanvas` (+ its `Ruler`) — the thin half. It replaces the empty Arrangement dock in `MainShell`, paints the grid at the three grid tokens and the labels in `readoutFont`, and forwards wheel, click and drag to the view-model.

## Decisions

- **The grid ladder.** Below a beat: 1/64, 1/32, 1/16, 1/8 notes. Above it: one bar, then bars doubling to sixteen. `gridFor()` takes the finest rung whose lines are >= 18 px apart. The ruler asks the same ladder with a wider minimum (48 px) and never finer than a beat, so labels thin out one rung ahead of the lines. A label on a bar multiple is the bare bar number ("3"), anything else is bar.beat ("1.2") — which is why bar 3 reads "3" at beat 8.0 at every zoom that labels it.
- **Zoom clamps.** The view state's existing 1–2000 px/beat range is what keeps both ends of that ladder reachable: at 2000 the 1/64 rung is chosen, at 1 the sixteen-bar rung still has room. Asserted at both ends rather than restated as a new constant.
- **Vertical zoom is the track rows.** Ctrl+Shift+scroll is `ViewState::scaleTrackHeights`, not a new VIEW attribute — spec 535bbo persists the horizontal zoom and the two scrolls, and a track height is already per-track state. Nothing is visible until tracks exist; noted on s1jzd4.
- **The zoom keys are Commands.** `zoomIn`, `zoomOut`, `zoomToFit` joined the one keyboard policy, registered by a new `timelineShortcuts()` that the shell adds to `panelShortcuts()`. `+` is registered with and without Shift, and `=` beside it: a punctuation key says nothing about whether Shift produced the character or modified it.
- **The wheel's notch is converted at the component edge.** JUCE reports a fraction (X11's notch is 50/256 of it); the view-model's conventions are written in notches, which is what a producer turns.

## Facts for a reviewer

- **The playhead takes no lock in paint.** `SessionClock` reads `Session::playbackPositionSeconds`, which is `TransportControl::getPosition()` — a `CachedValue` read of the position the playhead publishes. The canvas polls it at 30 Hz and repaints only the two columns the line moved between.
- **A test about the playhead moving needs a real device.** The transport reads 0.0 s forever while blocks are pushed through the hosted interface, and reads 0.0 s through the first `pumpMessages` call however long it runs. Recorded in `docs/ENGINE_NOTES.md` under further facts; the test skips where there is no device and pumps until the position moves.
- The nine criteria are covered by `tests/TimelineGeometryTests.cpp` (round-trip, grid, zoom anchor, lines, labels, clamps, fit), `tests/ArrangementViewTests.cpp` (the four wheel gestures, the zoom keys, the playhead against a real session, the ruler click leaving `undoNames()` holding only the producer's Action), `tests/ProjectViewTests.cpp` (63.5 px/beat and beat 12.25 back exactly, project not dirtied), `tests/ShortcutTests.cpp` and `tests/gui/MainShellTests.cpp` (the key reaching the timeline through the window).
- Verified in the running app as well as in tests: the ruler labels bars 1–7 at the default 30 px/beat with beat lines between them, and the playhead stands at beat 0.
- `an undo during a take neither stops it nor moves the playhead` (RecordingTests) failed twice while this slice's checks were run with the app and a screen capture contending for the audio device, then passed three times alone and twice in a full sweep. Same shape as issue ax88i4, and noted there.

All four checks pass: format clean, lint clean, 157/157 tests, and the app runs.
