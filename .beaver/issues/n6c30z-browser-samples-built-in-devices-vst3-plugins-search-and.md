---
id: n6c30z
title: 'Browser: samples, built-in devices, VST3 plugins, search and favorites'
state: todo
priority: medium
depends_on:
    - 2ch0cm
    - aty85a
parent: 535bbo
created: 2026-08-12T03:51:04Z
updated: 2026-08-12T03:51:04Z
---

## What to build

The left dock: everything the producer can put into a project, one drag away. Sections for the sample folders the producer has chosen, the two built-in instruments, the three built-in effects, and the scanned VST3 plugins. A search box filters across all sections, and any item can be favorited into a section of its own that persists across projects.

Dragging is the insert gesture: an instrument onto a track makes it that track's instrument, an effect onto a track or onto a mixer strip's insert chain adds it at the drop position, and a sample onto the timeline creates an audio clip at the drop point, snapped. Every drop is one Action. The sample folders and the favorites are app-global settings, so they follow the producer between projects; the folder list is managed from the Interface tab.

## Acceptance criteria

- [ ] The browser lists sections for sample folders, built-in instruments, built-in effects, and scanned VST3 plugins; a folder with no readable content shows as empty rather than missing.
- [ ] Search, worked: typing "rev" shows the built-in reverb and every sample whose name contains "rev", with the non-matching sections hidden; clearing the box restores the full tree with the previously expanded folders still expanded.
- [ ] Favoriting an item adds it to the favorites section and survives closing the app and opening a different project; unfavoriting removes it.
- [ ] Dropping a sample on a track lane at 3.4 beats with grid = 1 beat creates an audio clip on that track starting at beat 3.0, referencing the file project-relative, as exactly one Action; undo removes it.
- [ ] Dropping an instrument on a MIDI track sets that track's instrument as one Action; dropping an effect on a track or between two plugins of a mixer strip's insert chain inserts it at that position as one Action.
- [ ] A dropped built-in instrument plays when its track receives notes, and a dropped effect is audible in the chain.
- [ ] Dropping onto an invalid target — an instrument onto an audio track, anything onto empty dock space — cancels with no Action and shows no error dialog.
- [ ] Adding and removing sample folders from the Interface tab updates the browser live and persists across restarts.
- [ ] The VST3 section lists the plugins the foundation's scan found and reflects a rescan without a restart; a plugin the scan rejected does not appear.
- [ ] Dragging a VST3 instrument or effect inserts it exactly as a built-in does, and its editor opens from the mixer's insert chain.
