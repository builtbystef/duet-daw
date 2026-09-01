---
id: bbevsg
title: Arrangement clip-edit previews, handles, and invalid targets
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
parent: wf0i5v
created: 2026-09-01T18:37:04Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Make the existing smart-tool clip gesture state fully observable before mouse-up and paint that state. Track-reorder/autoscroll is separate.

## Settled feedback

- `ArrangementView` exposes `ClipGestureDrawing`: original rectangles, pending rectangles, kind, copy flag, destination track/time, snapped/unsnapped value, and validity.
- Move/copy shows origin at 35% plus pending clip; trim/loop keeps the unchanged edge fixed and shows pending edge/length. A numeric tooltip reads destination bar.beat or resulting length.
- Valid destination rules reuse the actual completion policy: audio only to Audio, MIDI only to MIDI; no Group; negative time and absent track are invalid. Invalid pending clips use semantic-danger outline and mouse-up emits no Action.
- Hover zones are 6 logical pixels at trim edges and 10x16 at loop handle, scaled by Appearance. Cursor is left-right resize for trims/loop, drag for body, copy-drag under Ctrl, forbidden for invalid destination.
- Escape/focus loss/project replacement cancel and restore with no Action.

## Acceptance and tests

- [ ] Paintless tests assert exact pending geometry and validity for move, copy, both trims, loop, snap/Alt, and wrong-kind tracks.
- [ ] Existing one-gesture/one-Action contracts remain; invalid/cancel is digest and undo neutral.
- [ ] Component tests drive hover cursor, pointer modifiers, focus loss, and visible overlay composition without pixel assertions.

Start in `ArrangementView.h/.cpp`, `ArrangementCanvas.h/.cpp`, and `SmartToolTests.cpp`. Run all AGENTS.md checks before closing.
