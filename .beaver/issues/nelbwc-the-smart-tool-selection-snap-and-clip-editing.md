---
id: nelbwc
title: 'The smart tool: selection, snap, and clip editing'
state: todo
priority: high
depends_on:
    - s1jzd4
parent: 535bbo
created: 2026-08-12T03:49:43Z
updated: 2026-08-12T03:49:43Z
---

## What to build

The one tool the whole app edits with — there is no tool palette, and what a gesture means comes from where it starts and which modifier is held. This slice delivers the two view-models every surface reuses: the selection model and the snap arithmetic, plus the clip gestures on the arrangement.

Selection: click selects, dragging on empty space rubber-bands, Ctrl+click toggles, Shift+click extends, Ctrl+A selects everything on the focused surface, Escape clears. There is one current selection, and later slices read it (the Collaborator's context chip among them).

Snap is on by default against the visible adaptive grid, and holding Alt bypasses it mid-drag — pressing or releasing Alt during a drag changes the outcome from that moment, not only at the start. Clip gestures: drag moves, Ctrl+drag copies, dragging an edge trims, the loop handle extends a clip by looping, Delete deletes, double-clicking a MIDI clip opens the piano roll, and double-clicking an empty MIDI lane creates a one-grid-unit clip. The clip context menu offers Cut, Copy, Paste, Duplicate, Delete, Rename, and colour, with Paste alone on the empty timeline; the clipboard behind them lands here. Each completed gesture is exactly one Action, and a gesture abandoned with Escape emits none.

## Acceptance criteria

- [ ] Selection, worked: click clip A → {A}; Ctrl+click B → {A, B}; Ctrl+click A → {B}; Shift+click D with B selected → B through D; Escape → {}. Ctrl+A selects every clip on the arrangement when it has focus, and only the notes of the piano roll when that has focus.
- [ ] Rubber-band from empty space selects every clip its rectangle intersects and clears the previous selection unless Ctrl is held.
- [ ] Snap, worked: grid = 1 beat, a drag landing at 3.30 beats commits at 3.0; the same drag with Alt held commits at 3.30; releasing Alt before the mouse-up commits at 3.0 again.
- [ ] A completed clip drag emits exactly one Action named for the move, carrying the snapped destination; the same drag with Ctrl held emits a copy, leaving the original in place, and the copy is the new selection.
- [ ] Dragging a clip's left or right edge trims it, snapped, as one Action; the clip's content stays aligned to the timeline rather than sliding with the edge.
- [ ] The loop handle extends a clip by whole repetitions of its content as one Action, and undo returns it to its pre-loop length digest-exactly.
- [ ] Dragging a clip to another track moves it there as one Action; dropping outside any track lane cancels with no Action.
- [ ] Escape during a move, copy, or trim abandons the gesture: the clip returns to its original position and no Action is emitted.
- [ ] Deleting a multi-clip selection is one Action, and one undo restores every deleted clip.
- [ ] Double-clicking an empty MIDI lane at 5.4 beats with grid = 1 beat creates an empty MIDI clip from beat 5.0 lasting one beat, as one Action; double-clicking a MIDI clip opens it in the piano roll and emits no Action.
- [ ] Cut, Copy, Paste, and Duplicate work across tracks: copying a clip and pasting at the playhead on another track produces one Action, and pasting into empty space on the timeline works from that surface's context menu.
- [ ] Every context-menu entry mirrors an Action reachable by a direct gesture too — nothing exists only in a menu.
