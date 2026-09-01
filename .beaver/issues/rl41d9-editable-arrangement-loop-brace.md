---
id: rl41d9
title: Editable arrangement loop brace
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

Add loop-range geometry/gestures to `ArrangementView` and draw/hit-test them in the pinned ruler. Sections and general clip previews are separate tasks.

## Settled behavior

- The ruler always draws the stored loop range, enabled or disabled. A 10-logical-pixel brace below time labels has start handle, body, and end handle; enabled uses accent, disabled uses muted text/border.
- Edge/body drags work in timeline beats through `TimelineGeometry`. Edges round to the active grid, body preserves length, Alt bypasses, and the minimum range is one current grid subdivision.
- During drag the transient brace follows the pending range. Escape restores; mouse-up calls `Session::setLoopRangeSeconds` once. Loop range is transport configuration: no Action, dirty project change, or undo.
- Persistence continues to store the model's musical loop range and tempo changes continue to move its seconds with the music.

## Acceptance and tests

- [ ] Literal geometry tests cover start/body/end hit zones, snap, Alt, minimum length, and offscreen ranges.
- [ ] Set bars 1 through 8 in 4/4, enable Loop, and headless playback wraps at the exact boundaries with no content beyond the end heard.
- [ ] Save/reopen and tempo change preserve the same musical bars; range gestures do not touch undo.
- [ ] Component tests cover pointer routing and Escape, not paint.

Start in `ArrangementView`, `ArrangementCanvas::Ruler`, timeline tests, and existing transport tests. Run all AGENTS.md checks before closing.
