---
id: 3rd6lu
title: Automation lanes under tracks
state: todo
priority: medium
depends_on:
    - nelbwc
    - aty85a
parent: 535bbo
created: 2026-08-12T03:50:16Z
updated: 2026-08-17T04:09:35Z
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
