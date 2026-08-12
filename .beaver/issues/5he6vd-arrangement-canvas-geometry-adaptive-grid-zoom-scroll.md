---
id: 5he6vd
title: 'Arrangement canvas: geometry, adaptive grid, zoom, scroll, playhead'
state: todo
priority: high
depends_on:
    - fcsez4
    - 4r7nlj
parent: 535bbo
created: 2026-08-12T03:49:06Z
updated: 2026-08-12T03:49:06Z
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
