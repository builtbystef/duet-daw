---
id: 3vwusn
title: Autosave and crash recovery
state: done
assignee: agent
priority: medium
depends_on:
    - 1c8sjh
parent: b1j3me
created: 2026-08-11T01:51:16Z
updated: 2026-08-24T07:29:48Z
---

## What to build

Autosave per spec b1j3me as amended from UI grill s11o4w (2026-08-10): the interval is a setting — off, 2, 5, or 10 minutes, default 10 — stored app-globally, not per project. When the project is dirty, the timer writes a single recovery file inside the project folder, never touching the project file. On the next open, a recovery file newer than the project file is offered for restore. A plugin crash costs minutes, not a session. (The Settings > Interface panel that surfaces the option belongs to the UI spec; this slice exposes the setting through the facade and reads it.)

## Acceptance criteria

- [ ] The autosave interval is an app-global setting with values off/2/5/10 minutes and default 10; changing it takes effect without restart.
- [ ] Worked example: with the project dirty, an elapsed interval writes the recovery file beside the project file — the project file's bytes and mtime are unchanged; while clean, the timer writes nothing.
- [ ] Repeated autosaves reuse the single recovery file (no accumulation).
- [ ] Kill the app (SIGKILL) with unsaved edits after an autosave tick, reopen the project → a restore offer appears; accepting restores the autosaved edits; declining opens the last explicitly saved state.
- [ ] After an explicit save, reopening offers no restore (the recovery file is no longer newer, or is removed).
- [ ] Autosave uses the same snapshot-save path as explicit save (atomic write, redo stack survives).
- [ ] The setting is off → the timer never fires, verified by test with a mocked clock or shortened interval seam.

## Notes

**agent** — 2026-08-24T07:29:48Z

Implemented autosave and crash recovery.

- Added the app-global autosave setting facade with off/2/5/10-minute choices and a ten-minute default. The shell re-reads the store on its existing timer, so a changed value updates the open project without restart.
- `Project` now has a steady-clock scheduling seam. Dirty elapsed ticks atomically replace one `project.tracktionedit.recovery` snapshot; clean and disabled ticks write nothing and leave the explicit project file untouched.
- Explicit save and autosave share `writeSnapshot`, including parameter-blob capture. Autosave leaves dirty state and redo intact; explicit save clears dirty state and removes recovery.
- A recovery newer than the project file is exposed through the persistence facade. The app offers Restore/Open Saved; restore opens the recovery as unsaved state, while decline opens the explicit save and discards recovery. Coarse filesystem mtimes are ordered explicitly so a successful recovery write is always newer.
- Added facade tests for every interval, mocked elapsed time, clean/off behavior, project bytes and mtime, repeated replacement, redo survival, restore/decline after the prior session disappears without saving (the headless seam equivalent of SIGKILL), and explicit-save cleanup.
- Updated ARCHITECTURE.md.

Checks: format clean; full lint clean; full Debug build clean; 182/182 tests passed (8 existing hardware-dependent skips).
