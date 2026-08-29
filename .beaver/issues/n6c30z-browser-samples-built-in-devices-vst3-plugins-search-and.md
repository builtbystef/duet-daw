---
id: n6c30z
title: 'Browser: samples, built-in devices, VST3 plugins, search and favorites'
state: done
assignee: claude
priority: medium
depends_on:
    - 2ch0cm
    - aty85a
parent: 535bbo
created: 2026-08-12T03:51:04Z
updated: 2026-08-29T11:17:16Z
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

## Notes

**claude** — 2026-08-29T11:17:16Z

Built (2026-08-29). The left dock is a real surface: duet::gui::Browser is the paintless view-model, BrowserCanvas the thin half, and every drop ends in one Action on the vocabulary layer.

What is there
- Sections, in this order: Favourites, Instruments (4OSC, Sampler), Effects (EQ, Compressor, Reverb), VST3, then one section per chosen sample folder. A folder with nothing readable under it is a section with no items rather than a missing section. The device names are the ones the mixer's insert menu already uses, so one thing has one name.
- Search filters every section by name, case-insensitively, hides the sections that match nothing, and shows what is left open; clearing it restores the tree with the producer's own open/closed state intact.
- Favourites and sample folders live in the app-global store (keys browser.favourites, browser.sampleFolders), so both outlive the app and follow the producer between projects. Favourites keep the order they were made in, that being the only order the producer chose.
- Drops: a sample becomes an audio clip, snapped to the grid unless Alt is held (Insert Audio Clip); an instrument becomes what a MIDI track plays, replacing the one it had (Set Track Instrument); an effect goes into a chain at the position it was dropped, or at its end (Insert Plugin). A drop with nowhere to land does nothing at all - no Action, no dialog.
- The Settings window's Interface tab manages the folder list, and the dock is the same Browser object, so a folder added there is in the dock before the window closes.

Decisions made here
- A dropped sample is imported into the project's audio/ subdirectory first, so the clip's stored reference is project-relative and the folder stays self-contained (ADR 0005). The copy is the persistence facade's (Project::importAudioFile), and the browser is a view-model that knows no facade, so the host hands the import over as Browser::setSampleImporter - its first production caller.
- The Master is a strip with a chain like any other: an effect can be dropped on it, and an instrument or a sample cannot.
- A folder section starts closed and the four device sections start open: a sample library is the one section that can hold thousands of things. What the producer changes stands for the run; the spec puts neither in VIEW nor in the app-global store, so it is neither.
- A sample is listed by extension (.wav .aiff .aif .flac .ogg .mp3), read recursively, and named by its path relative to the chosen folder. The read happens on refresh() and never while a surface paints.
- MainShell now takes the app-global store (it owns the Browser) and is the window's juce::DragAndDropContainer; ArrangementCanvas and MixerCanvas are drop targets. A drag carries the item's identity behind a "browser:" prefix, so a target can tell a browser drag from anything else without knowing the dock's component type.

Facts for a reviewer
- tests/BrowserTests.cpp covers the view-model at the Action seam, including the audible half: a dropped 4OSC renders the note at 440 Hz and a dropped Reverb puts a tail where the instrument alone left silence.
- tests/gui/BrowserCanvasTests.cpp covers the thin half - the shell's left dock, and what the two drop targets do with a drag's coordinates.
- tests/gui/GuiSettingsHome.cpp is new: a component test that opens a project takes the engine with it, and the engine keeps app-global settings in the user's configuration directory. The gui suite now points XDG_CONFIG_HOME at a temp directory for the run, as duet_tests' own main already did, so it never writes into the producer's real settings.
- All four checks pass: format, lint sweep, full build, 512/512 tests.
