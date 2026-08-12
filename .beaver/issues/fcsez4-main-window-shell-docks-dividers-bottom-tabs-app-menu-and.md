---
id: fcsez4
title: 'Main window shell: docks, dividers, bottom tabs, app menu, and the VIEW tree'
state: todo
priority: high
depends_on:
    - xxv9ng
    - 1c8sjh
parent: 535bbo
created: 2026-08-12T03:48:32Z
updated: 2026-08-12T03:48:32Z
---

## What to build

The single main window the whole interface lives in. A transport-bar strip across the top (empty of controls until its own slice), the arrangement in the center, a browser dock left, the Collaborator dock right, and a resizable, collapsible bottom panel with Piano Roll and Mixer tabs. Draggable dividers separate all three docks from the arrangement. One "Duet" app menu button replaces any menu bar; it carries the panel toggles here, and project commands arrive with the lifecycle slice. Plugin editors will be the only floating windows; nothing else tears off.

This slice also establishes the two places UI state lives. Window geometry is app-global. Per-project view state is a `VIEW` child of the DUET tree, written with no UndoManager so it never reaches producer undo, and captured at save time so that resizing a panel never dirties the document. The shape, extended by later slices with their own attributes:

```
DUET
└─ VIEW  (covered by duetSchemaVersion like the rest of DUET)
   ├─ @hZoomPxPerBeat, @hScrollBeats, @vScrollPx
   ├─ @browserVisible, @collaboratorVisible, @bottomVisible
   ├─ @browserWidthPx, @collaboratorWidthPx, @bottomHeightPx, @bottomTab ("pianoRoll"|"mixer")
   └─ TRACKVIEW (one per track)  @trackRef, @heightPx, @lanesExpanded
```

Keyboard dispatch policy lands here too, because this is the first surface with keys: bare-letter shortcuts are inactive while a text field has focus, and later slices register their keys through the same policy.

Prototype finding worth the implementer's time (r4m858): a `setBounds` call with unchanged bounds skips the resize callback, so switching the bottom tab must refresh the newly visible surface explicitly.

## Acceptance criteria

- [ ] One main window shows the transport strip, the arrangement, both side docks, and the bottom panel; dragging a divider resizes the neighbouring dock, and each dock can collapse and reopen at its previous size.
- [ ] The Duet menu button opens a menu carrying the panel toggles; there is no menu bar and no floating panel.
- [ ] Keys B, C, E toggle the browser, Collaborator, and bottom panel; P and X select the Piano Roll and Mixer tabs. With a text field focused, those same bare letters type their character and toggle nothing.
- [ ] Switching the bottom tab shows the newly selected surface correctly laid out, including when the panel's bounds are unchanged by the switch.
- [ ] View round-trip, worked: set browser width 260 px, Collaborator hidden, bottom panel 320 px on the Mixer tab; save, reopen the project → the layout returns with those exact values, and the reopened document is not dirty.
- [ ] A VIEW write never appears on the undo stack: collapse a dock, resize it, switch tabs, then undo — the last edit Action is what undoes, and the layout does not change.
- [ ] Changing only view state and closing the project prompts nothing; the document is dirty only if an Action was performed.
- [ ] Window position and size are app-global: they restore on relaunch regardless of which project opens.
- [ ] Surfaces render on the software renderer by default, with a per-surface hardware-accelerated context available as an opt-in escape hatch that changes nothing visible when enabled.
