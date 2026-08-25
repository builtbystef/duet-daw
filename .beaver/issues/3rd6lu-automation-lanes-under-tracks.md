---
id: 3rd6lu
title: Automation lanes under tracks
state: done
assignee: claude
priority: medium
depends_on:
    - nelbwc
    - aty85a
parent: 535bbo
created: 2026-08-12T03:50:16Z
updated: 2026-08-25T19:53:59Z
---

## What to build

Parameters move over time. A track header expands to reveal automation lanes beneath it, each lane bound to a target — track volume, track pan, or a parameter of a plugin on that track — chosen from the lane's own picker. A lane draws its points joined by linear segments over the same timeline geometry the arrangement uses, so the grid, zoom and scroll are shared.

Editing follows the smart tool: double-click adds a point, dragging moves it with horizontal snapping (values are continuous and never snap), right-click removes it. Segments are linear in milestone one, but the path from the UI to the model must not foreclose per-point curvature, which the engine already supports and milestone two will expose. Which lanes are expanded persists per project in the VIEW state.

## Acceptance criteria

- [ ] A track header expands and collapses its automation area; the expanded state survives save and reopen, and is not on the undo stack.
- [ ] A lane's target picker offers track volume, track pan, and the parameters of that track's plugins; switching a lane's target redraws it against that parameter's own value range.
- [ ] Adding a point, worked: with grid = 1 beat, double-clicking at 2.4 beats at 75% of the lane's height adds a point at beat 2.0 with the value that height represents, as exactly one Action; undo removes it digest-exactly.
- [ ] Dragging a point snaps horizontally to the grid and moves freely vertically; Alt held during the drag bypasses the horizontal snap. The completed drag is one Action.
- [ ] Right-clicking a point removes it as one Action; right-clicking empty lane space does not.
- [ ] Points render joined by straight segments, and the value the engine plays between two points matches linear interpolation: points at beats 0.0 (0.0) and 4.0 (1.0) give 0.5 at beat 2.0.
- [ ] The value written for a point survives a save and reopen exactly, and the stored form carries no assumption that segments are linear — adding curvature later changes no existing data.
- [ ] Automating a parameter while the transport rolls moves the parameter audibly, and the lane's drawn curve matches what the mixer or plugin shows.
- [ ] Deleting a track removes its lanes and their expanded state with it.

## Notes

**claude** — 2026-08-25T19:53:42Z

Done. Automation lanes under the arrangement's tracks, at both agreed seams.

**What was built**

- `duet::gui::AutomationLanes` (paintless) — the automation area a track header opens beneath its row. It owns which lanes a track has, what each is bound to, the geometry that says what a height in a lane is worth (`valueAtY` / `yForValue`), the hit-testing (`pointIndexAt`), the straight-segment reading of a curve (`valueAtBeats`), and the three gestures: `addPoint`, the `begin/update/complete/cancel` point drag, and `removePoint`. It borrows the arrangement's own `TimelineGeometry`, so a lane, its grid, its zoom and its scroll are the ones the clips above it are drawn on.
- `ViewState` gained a lane list per `TRACKVIEW` row (`LaneView`: target kind, plugin, parameterId, heightPx), written as `LANE` children under the row that already carried `lanesExpanded`. Lanes are the producer's view and not their work, so they live there, are captured by a save, are on no undo stack, and die with the row when the track goes.
- `TrackDrawing` gained `automationY` / `automationHeight`: the arrangement says where a track's automation area starts and how tall it is, and the lanes count their own coordinates from that point. `contentHeightPx` and the rows below an open area move down by it.
- `ArrangementCanvas` — the disclosure triangle at the left of each track header, the lanes painted under the row (target name as the lane's own picker, points joined by straight segments, flat runs out to both edges), and the input: double-click adds, drag moves, right-click removes, the header's name opens the target/add-lane/remove-lane menu. Escape reaches the drag through `Command::cancel`, like a clip gesture.

**Vocabulary extended, and why**

- `AutomationPoint` now carries `curvature`, read from and written to the engine's per-point curve value. This is what makes "the stored form carries no assumption that segments are linear" a fact rather than a hope: the point states its own straightness, milestone one always writes zero, and a drag carries whatever it found back untouched — asserted by "a drag carries a point's curvature through untouched". Adding curved segments later changes a value, not the data.
- `Session::automationValueAt (target, seconds)` — what the engine plays at a time, in the units the points are written in. Without it a test cannot say the lane draws what the engine plays, which is half of two criteria.

**Decisions**

- A lane's range is its target's own: the fader's -60..+6 dB (`Mixer::faderMinimumDb/MaximumDb`, so a volume lane and the mixer fader are one control drawn twice), -1..+1 for pan, and a plugin parameter's own min..max. Switching a lane's target redraws it against the new range.
- Opening a track's automation area for the first time gives it one lane on the track's volume. An area with nothing in it says nothing about the track.
- A lane's top is its target's largest value; `y` maps by `fromTop = (y - lane.y) / lane.height`, so three quarters down a pan lane is exactly -0.5 and the worked example lands on round numbers rather than on rounding.
- Value never snaps, position does: `snapBeats` with the arrangement's own grid, bypassed while Alt is held at the moment the drag is read.

**What the tests say**

`tests/AutomationLaneTests.cpp` (view-model seam) covers all nine criteria: the area opening and closing without touching the undo stack or the digest; the picker's contents and per-target ranges; the worked add (grid 1 beat, 2.4 beats -> 2.0, three quarters down -> -0.5) as one Action with a digest-exact undo; the drag's horizontal snap, free vertical, Alt bypass, single Action, and abandoned-with-no-Action case; right-click on a point versus on empty lane space; linear interpolation agreeing with `automationValueAt` on the worked 0.0/4.0 -> 0.5 example; a save and reopen bringing back the lane, its target and a point value of exactly 0.75; a curve drawn point by point through the lane changing a rendered tone's level by what the lane's own points say (ADR 0006 feature assertion, +/-0.5 dB); and a deleted track taking its lanes and its open area with it. `tests/AutomationOpsTests.cpp` covers the two model additions.

**Not covered by a test, deliberately**

The canvas. Spec 535bbo settles that paint code stays untested and look is judged live; the wiring was exercised by running the app, which starts and stays up with the change.

**Checks**

Format clean, full lint sweep clean, 296/296 CTest entries pass with the 8 expected hardware skips. The sandbox needs `XDG_RUNTIME_DIR` set for Catch2's generated test listing; that is the machine, not the tree.

**Published**

Issue s2e041 (bug, low): `"Duet — "` and `"Piano Roll — "` reach `juce::String` as 8-bit text and assert continuously in a Debug run. Found while running the app for this slice; it predates it and is unrelated, so it was not fixed here.
