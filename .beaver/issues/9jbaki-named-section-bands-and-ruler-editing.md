---
id: 9jbaki
title: Named section bands and ruler editing
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

Expose `Session::sections()` as editable labelled bands in the arrangement ruler.

## Settled section policy

- Bars are inclusive as `SectionInfo` defines. Sections are sorted by start bar, may leave gaps, and may not overlap. Names trim surrounding whitespace and must be non-empty.
- `Add Section` at a ruler context point starts at that bar and spans up to four bars, clamped before the next section; if no positive range fits, the route is disabled. Default names are `Section N`.
- Drag either edge by whole bars; the body moves by whole bars preserving inclusive length. Neighbour and bar-1 clamps prevent overlap/non-positive ranges. Alt has no effect because sections are bar-defined.
- Double-click label renames; context menu adds/removes. Add, rename, resize/move, and remove each replace the whole section list in one appropriately named Action.
- Bands occupy a fixed row above the loop brace and ellipsize only with a tooltip carrying the full name/range.

## Acceptance and tests

- [ ] Paintless geometry tests cover sort, gaps, neighbour clamps, inclusive lengths, move, rename validation, and hit zones.
- [ ] Every completed gesture emits one Action; cancel/no-op emits none; undo and save/reopen are digest-exact.
- [ ] Component tests cover menus, text focus, pointer routing, and Escape.

Start in `ArrangementView/Canvas`, `Session::sections`, and `ArrangementViewTests.cpp`. Run all AGENTS.md checks before closing.
