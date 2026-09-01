---
id: nt104h
title: Track insertion marker and edge autoscroll during arrangement drags
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
depends_on:
    - bbevsg
parent: wf0i5v
created: 2026-09-01T18:37:04Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Complete long-project dragging: show the track reorder destination and autoscroll held clip, track, and automation gestures without changing the musical point under the pointer.

## Settled behavior

- Track drag exposes a horizontal insertion marker before/after the row whose midpoint is crossed; marker follows vertical scroll and mouse-up uses that exact index.
- Within 24 logical pixels of a timeline edge, a held clip/automation drag scrolls horizontally up to 12 px per 60 Hz tick proportional to edge penetration. Within 24 px of top/bottom, track reorder scrolls vertically by the same rule.
- Autoscroll is driven by a supplied tick/time seam, not wall-clock sleeps. After each scroll, recompute the pending model destination so the held item's pointer offset and musical target remain invariant.
- Stop at content/project bounds. Escape, mouse-up, focus loss, and project replacement stop the timer immediately.

## Acceptance and tests

- [ ] Deterministic tick tests cover speed, bounds, invariant pointer offset/musical beat, and insertion indices while scrolling.
- [ ] Component tests prove timers start/stop only for active edge drags and draw one insertion marker.
- [ ] No scroll tick emits an Action; completion still emits exactly the original gesture Action.

Start in `ArrangementView/Canvas`, `ViewState`, and smart-tool/component tests. Run all AGENTS.md checks before closing.
