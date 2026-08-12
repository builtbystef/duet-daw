---
id: ce17ym
title: 'Project lifecycle: an untitled project is a real folder from the start'
state: todo
priority: high
depends_on:
    - fcsez4
    - 3vwusn
parent: 535bbo
created: 2026-08-12T03:48:47Z
updated: 2026-08-12T03:48:47Z
---

## What to build

The app always has a real project open. On launch it reopens the last project when one exists and its folder is still present; otherwise it creates a new untitled project in the default projects directory. There is no start screen. An untitled project is a genuine project folder from the moment it exists, so recording and the foundation's in-folder autosave and recovery work before any save; the first Save As renames or relocates that folder. Save As copies the whole folder — edit file and audio — and the session continues in the copy, keeping a project a self-contained folder.

The Duet menu gains its project commands: New, Open, Save, Save As, and Recent. Closing with unsaved changes prompts Save / Discard / Cancel. A new project seeds one instrument track carrying the built-in synth and one audio track. The Interface tab gains two settings: the projects directory and the autosave interval — off, 2, 5, or 10 minutes, default 10, which amends the foundation's fixed 5.

## Acceptance criteria

- [ ] Launch with no prior project → a new untitled project exists on disk as a folder in the projects directory, with its audio subfolder, before the producer does anything; the window title shows the untitled name.
- [ ] Launch with a prior project whose folder is present → that project reopens with its view state; launch with the folder gone → a new untitled project, no error dialog, no start screen.
- [ ] A new project opens with exactly two tracks: one instrument track carrying the built-in synth, one audio track.
- [ ] Save As, worked: an untitled project with one recorded audio file, saved as a new name in a new location → the destination folder holds the edit file and a copy of that audio file, the source folder is untouched, subsequent saves write to the destination, and the window title shows the new name.
- [ ] Numbering: with `Untitled 1` already present in the projects directory, the next untitled project is created as `Untitled 2` rather than overwriting.
- [ ] Closing with unsaved changes prompts Save / Discard / Cancel; Cancel aborts the close and leaves the document dirty, Discard closes without writing, Save writes then closes.
- [ ] New and Open on a dirty project run the same prompt first.
- [ ] The autosave interval setting reaches the foundation's autosave: set to 2 minutes → an autosave lands in the project folder on that cadence; set to off → none lands; the default in a fresh install is 10 minutes.
- [ ] Changing the projects directory affects only projects created afterwards; existing projects stay where they are.
- [ ] Recent lists the recently opened projects, drops entries whose folder no longer exists, and opening one restores it with its view state.
