---
id: 1c8sjh
title: 'Persistence core: project folder, DUET tree, snapshot save, in-app New/Open/Save'
state: todo
priority: high
depends_on:
    - quiwf3
parent: b1j3me
created: 2026-08-11T01:50:53Z
updated: 2026-08-17T04:12:26Z
---

## What to build

The persistence facade per spec b1j3me / ADR 0005 and branch prototype/duet-persistence (27/27): a project is a self-contained folder (edit file plus an audio subdirectory, paths project-relative); Duet's own data lives in a DUET child tree in the same file; saving is a snapshot — copy the state, apply the parameter blobs to the copy with no UndoManager, write the copy atomically — never the engine's own flush-and-save path (hazards 3 and 4). EditItemID-derived refs are the durable keys. Wired to minimal app chrome — New/Open/Save and a title-bar dirty marker — so the whole path demos end to end. (Schema versioning and autosave are their own slices.)

## Acceptance criteria

- [ ] Create project → a folder appears containing the edit file and an audio subdirectory; open project → the same state comes back (canonicalized digest of saved state equals digest after reload).
- [ ] Importing an audio file copies it into the project's audio subdirectory and the clip stores a project-relative reference; after moving the whole project folder elsewhere on disk, the project opens and the clip still plays non-silence.
- [ ] Worked example (spec): set a plugin parameter explicitly, let automation diverge it during playback, save, reload → the explicit value is restored exactly; and the in-session redo stack survives the save (redo still works after saving).
- [ ] Duet-side data written into the DUET child tree persists across save/reload; facade refs remain valid for the same items after reload (EditItemID stability).
- [ ] The dirty flag: an Action marks the project dirty (title-bar marker appears); save clears it; undo back to the saved state is not required to clear it.
- [ ] Save is atomic: killing the app mid-save never leaves a corrupt project file (write-then-rename or equivalent, verified by test).
- [ ] In-app demo: New → edit → Save → quit → Open shows the edited project, launched via pw-jack.

## Notes

**claude** — 2026-08-17T04:12:26Z

Clarification (2026-08-17): the New/Open/Save chrome here is deliberately minimal scaffolding to exercise the persistence facade end to end; ce17ym later replaces it with the full Duet-menu lifecycle (Save As, Recent, close prompt, untitled-folder flow). Build it throwaway-thin. The in-app demo criterion is a manual check on the dev machine, recorded as a note — not an automated test.
