---
id: 0as8g4
title: Arrangement create/remove and edit routes for crossfades
state: todo
priority: low
labels:
    - session:task
    - roadmap:h0eir5
depends_on:
    - btqhk2
    - qgl6tk
parent: ie6bjp
created: 2026-09-01T18:39:45Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Expose explicit crossfades through selected overlapping clips and draw their relationship.

## Settled interaction

- With exactly two overlapping audio clips on one track selected, context menu `Create Crossfade` is enabled and defaults Equal Power. An existing pair offers Shape > Equal Power/Linear and Remove Crossfade.
- The overlap draws both envelope curves and a crossfade badge. Existing clip-edge move/trim gestures change overlap boundaries and preview the resulting crossfade before their one Action commits.
- Invalid/non-overlapping/mixed-track/MIDI selections explain why the route is unavailable and emit no Action. No automatic crossfade preference is introduced.
- Crossfade controls have explicit accessible names/tooltips and do not hide individual fade handles outside the overlap.

## Acceptance and tests

- [ ] Paintless selection/geometry tests cover create enablement, shape, remove, pending boundary changes, and dissolved overlap.
- [ ] Component tests cover menu/focus/cursor/composition; model audio tests remain the sound authority.

Start in `ArrangementView/Canvas`. Run all AGENTS.md checks before closing.
