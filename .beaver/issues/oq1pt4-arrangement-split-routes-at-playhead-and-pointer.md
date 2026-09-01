---
id: oq1pt4
title: Arrangement Split routes at playhead and pointer
state: todo
priority: low
labels:
    - session:task
    - roadmap:h0eir5
depends_on:
    - 6w0ffk
    - qljlof
parent: 7zuqxx
created: 2026-09-01T18:39:07Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Expose clip splitting through the arrangement selection/action seam.

## Settled interaction

- Ctrl+E splits every selected clip crossed by the playhead as one `Split Clips` Action. The clip context menu adds `Split at Pointer`; it splits the clip under the pointer at that pointer's current grid-snapped beat, with Alt bypass.
- Context split affects only the pointed clip unless it is within the current multi-selection, in which case every selected clip crossed by that time splits together.
- Right halves become selected and left halves leave selection, making the material after the cut ready to move. Clips not crossed remain selected as they were.
- Split is disabled/no-op at clip edges, outside clips, on Suggestion ghosts, or with no crossed selection. A vertical pending cut line and forbidden cursor communicate validity before choice.

## Acceptance and tests

- [ ] Paintless arrangement tests prove command/context scope, snap/Alt, selection result, mixed audio/MIDI multi-split, Action naming, and no-op cases.
- [ ] Shortcut tests preserve text-field suppression and Piano Roll focus behavior.
- [ ] Component tests cover context enablement and pending cut indicator composition.

Start in `ArrangementView/Canvas` and `Shortcuts`. Run all AGENTS.md checks before closing.
