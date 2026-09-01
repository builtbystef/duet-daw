---
id: jg62kc
title: Pinned Piano Roll ruler and musical grid
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
parent: kmsbfq
created: 2026-09-01T18:37:38Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Give `PianoRoll` ruler/grid drawing data from the same `TimelineGeometry` used by the arrangement and add a pinned ruler component region. Clipboard and audition are separate.

## Settled geometry

- The ruler is 22 logical pixels below the command strip and to the right of the piano keyboard. It draws the same visible bar labels and bar/beat/fine lines as arrangement at the same project beats.
- Notes are clip-relative but grid/ruler/playhead are project-relative; all x conversion goes through the existing `clipTimelineStartBeats()` plus shared geometry. No second scroll/zoom state is introduced.
- Dragging the ruler scrubs `TimelineClock`/Session playhead, clamped to project start, without an Action. Wheel/zoom over it forwards to Piano Roll timeline behavior.
- Ruler stays pinned during vertical key scrolling; horizontal scroll keeps ruler, grid, notes, playhead, and velocity bars aligned.

## Acceptance and tests

- [ ] Literal geometry at known zoom/scroll proves alignment for a clip that starts away from bar 1 and has content offset.
- [ ] Meter and adaptive-grid changes produce the same line beats/weights as arrangement.
- [ ] Scrub changes transport only and no undo/dirty state.
- [ ] Component tests cover pinned bounds and pointer/wheel forwarding, not pixels.

Start in `PianoRoll.h/.cpp`, `PianoRollCanvas.h/.cpp`, and timeline/Piano Roll tests. Run all AGENTS.md checks before closing.
