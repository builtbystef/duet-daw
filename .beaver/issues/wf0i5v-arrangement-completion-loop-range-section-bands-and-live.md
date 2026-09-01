---
id: wf0i5v
title: 'Arrangement completion: loop range, section bands, and live drag feedback'
state: todo
priority: high
labels:
    - spec
    - roadmap:yfpnps
parent: yfpnps
created: 2026-09-01T18:07:43Z
updated: 2026-09-01T18:42:01Z
---

## What to build

Make the arrangement's ruler and smart tool communicate the edit before it is committed. The ruler draws an editable loop brace and the project's named sections. Clip move/copy/trim/loop gestures, track reorder, and relevant automation gestures show their pending destination while held, with cursors and handles that reveal what a press will do.

## Acceptance criteria

- [ ] The ruler draws the current loop range whether loop playback is enabled or not, with distinct start, body, and end hit targets.
- [ ] Dragging either loop edge snaps to the current grid, moving the body preserves its length, Alt bypasses snap, and no valid loop can have a non-positive length.
- [ ] Loop-range edits are transport/view configuration and do not enter producer undo; the range survives save/reopen and Loop plays exactly that range.
- [ ] Worked loop: set bars 1-8, enable Loop, start near its end -> playback wraps to bar 1 and no clip outside the brace sounds.
- [ ] The ruler displays each named section as a labelled band over its bar range; add, rename, resize, reorder-by-time, and remove routes are available from the ruler.
- [ ] A completed section edit is one Action over the project's section list; undo and save/reopen restore it exactly.
- [ ] A clip move/copy draws the origin and pending destination, a trim/loop draws the pending edge/length, and an invalid target is visibly invalid before mouse-up.
- [ ] Track reorder shows an insertion marker. Dragging near a scrolled edge autoscrolls without changing the musical position under the held item.
- [ ] Hover cursors and visible handles distinguish move, left trim, right trim, and loop before mouse-down; these affordances scale with the interface.
- [ ] Escape during any transient arrangement gesture restores the pre-gesture picture and emits no Action; mouse-up emits exactly the one Action the existing smart-tool contract names.
- [ ] Suggestion ghosts remain non-interactive and visually distinct from producer drag feedback.

## Testing seam

Loop/section geometry and transient drawings belong to the paintless arrangement model. Component tests drive pointer zones, autoscroll, cursors, and Escape. Playback and persistence use the existing model and project seams.

## Notes

**agent** — 2026-09-01T18:42:01Z

AFK decomposition: loop rl41d9; sections 9jbaki; clip previews bbevsg -> reorder/autoscroll nt104h. This issue is a spec container.
