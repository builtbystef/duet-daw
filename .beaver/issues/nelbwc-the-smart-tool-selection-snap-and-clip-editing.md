---
id: nelbwc
title: 'The smart tool: selection, snap, and clip editing'
state: done
assignee: agent
priority: high
depends_on:
    - s1jzd4
    - q0lw4u
parent: 535bbo
created: 2026-08-12T03:49:43Z
updated: 2026-08-24T14:43:45Z
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

## Notes

**agent** — 2026-08-24T10:58:00Z

Contract ambiguity blocks implementation: the body and parent spec require the clip context menu to contain Cut, Copy, Paste, Duplicate, Delete, Rename, and colour, while the final criterion says every menu entry must also be reachable by a direct gesture. The specified direct gestures account for copy/duplicate (Ctrl+drag), Delete, move, trim, loop, and MIDI open/create, but specify no direct gesture or key for clip Rename or clip colour; Cut/Paste likewise only have conventional but unstated Ctrl+X/Ctrl+V keys. Adding F2, a colour-cycle key, a colour swatch gesture, or standard clipboard shortcuts would invent interaction policy. Options: (1) name the direct keyboard/pointer gestures for Rename, colour, Cut, and Paste; (2) amend the criterion so Rename and colour (and any other named exceptions) may be menu-only; or (3) define ‘direct gesture’ to include a specific existing route and identify it. User must decide, record the decision in an issue note, and remove needs-review.

**agent** — 2026-08-24T11:04:57Z

Decision (Target Producer): clip clipboard actions use Ctrl+X, Ctrl+C, and Ctrl+V; Duplicate uses Ctrl+D as well as Ctrl+drag; Delete uses Delete; Rename uses F2. Clip colour is the explicit context-menu-only exception—no obscure colour shortcut is added, and C remains the Collaborator-panel shortcut. For the final acceptance criterion, every context-menu entry mirrors a direct gesture or shortcut except clip colour; menu commands and direct routes must use the same Action path.

**agent** — 2026-08-24T11:31:54Z

Implementation is present and its focused tests pass, but the required full suite is blocked by q0lw4u: two existing VST3 persistence tests fail. I reproduced the known-list failure after fully stashing this issue's working tree, rebuilding untouched main, and running the test alone, so it is not caused by nelbwc. Published q0lw4u with the reproduction and acceptance criteria. Format and changed-file lint pass; the full suite otherwise passes 207 tests with 8 expected hardware skips. Per the implementation-session rule, q0lw4u must be completed before this issue can be finalized.

**agent** — 2026-08-24T14:43:45Z

Completed the smart-tool slice. Added the shared paintless Selection model and adaptive-grid snap function; arrangement clip selection/rubber-band, move/copy/trim/loop/cross-track/cancel gestures; multi-clip delete; MIDI clip open/create routes; clipboard paste/duplicate/cut/copy; clip rename/colour; context menus; and the decided Ctrl+X/C/V/D, Delete, F2, Ctrl+A, and Escape routes. Extended the model vocabulary only where these gestures needed cross-track move, edge trim with timeline-aligned content, rename, colour, and MIDI track identity; every completed edit remains one named Action, while cancellation and MIDI-open emit none. Clip colour is the recorded context-menu-only exception. SmartTool and shortcut tests cover both agreed seams, including worked selection/snap examples, exact undo for loop/delete, cross-track behavior, and clipboard Actions. Final checks pass: format, full lint, duet_tests and duet_app builds, and all 217 CTest entries (209 passed, 8 expected hardware skips). The sandbox required XDG_RUNTIME_DIR=/tmp for Catch2's generated listing file; this is not a repository or test failure.
