---
id: qgl6tk
title: Explicit audio crossfade relationship and equal-power render
state: todo
priority: low
labels:
    - session:task
    - roadmap:h0eir5
depends_on:
    - gfpfwl
parent: ie6bjp
created: 2026-09-01T18:39:45Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Add explicit crossfade state for exactly two overlapping audio clips on one track. Merely overlapping never creates it.

## Settled relationship

- `createCrossfade(first, second, shape)` requires same track, positive overlap, distinct audio clips, and no conflicting crossfade. Order is timeline order. Shapes are `equalPower` (default) and `linear`.
- Store the durable pair and each clip's pre-crossfade edge fade in the DUET tree. The relationship drives fade-out/in over the full current overlap. Equal-power uses complementary sine gains (each sqrt(1/2) at midpoint); Linear uses complementary linear gains.
- Moving/trimming either linked clip updates the envelope to the new overlap inside the same Action. If overlap becomes zero, dissolve the relationship and restore saved independent fades, clamped to current lengths.
- Removing explicitly also restores saved fades. Deleting/splitting either clip removes the relationship without dangling refs; undo restores all state.

## Acceptance and tests

- [ ] Create/remove/move/trim/delete/save/reopen and undo/redo preserve exact pair/restoration state.
- [ ] Equal correlated tones measure unity power at equal-power midpoint within tolerance; linear midpoint measures the specified summed gain; endpoints and continuity are asserted independently.
- [ ] Invalid pairs and plain overlaps create no relationship or Action.

Use the public Action seam and ADR 0006 feature assertions. Run all AGENTS.md checks before closing.
