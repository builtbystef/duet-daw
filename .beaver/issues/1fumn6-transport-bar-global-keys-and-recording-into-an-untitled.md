---
id: 1fumn6
title: Transport bar, global keys, and recording into an untitled project
state: todo
priority: high
depends_on:
    - ce17ym
    - 5he6vd
    - s1jzd4
parent: 535bbo
created: 2026-08-12T03:50:00Z
updated: 2026-08-12T03:50:00Z
---

## What to build

The strip across the top of the window becomes the transport: a bar/beat position readout beside a wall-time readout in tabular numerals, BPM, time signature, the grid-size control that drives the arrangement's snap grid, loop, metronome and follow-playhead toggles, a CPU percentage with a health indicator, undo and redo, and the project name.

The transport and global keys register through the shell's key policy: Space plays and stops, R records, L toggles loop, M the metronome, Home and End jump to start and end, F toggles follow-playhead, Ctrl+Z and Ctrl+Shift+Z undo and redo, Ctrl+S saves. All of them are inert while a text field has focus.

With record wired, story 10 closes end to end: a producer who has just launched the app can arm a track and record without saving anything first, because the untitled project is already a real folder.

## Acceptance criteria

- [ ] The position readout shows bars, beats and ticks alongside wall-clock time, both in tabular numerals, and both track the transport during playback without the layout shifting as digits change.
- [ ] BPM and time signature are editable and reach the model as Actions; changing 120 to 140 BPM re-labels the ruler and re-spaces the grid, and undo restores 120 with the ruler following.
- [ ] The grid-size control sets the snap grid: choosing 1/8 makes a drag landing at 3.30 beats commit at 3.5 rather than 3.0, and choosing Adaptive returns the grid to the ≥18 px rule.
- [ ] Loop, metronome and follow-playhead toggle from the bar and from L, M and F; with follow on, playback scrolls the arrangement to keep the playhead visible, and with it off the view stays put.
- [ ] Space starts and stops playback, Home and End move the playhead to the project start and end, and R starts and stops recording; none of these appear on the undo stack.
- [ ] Undo and redo buttons mirror Ctrl+Z and Ctrl+Shift+Z, are disabled when their stack is empty, and their tooltips name the Action they would undo or redo.
- [ ] The CPU readout tracks engine load during playback and its health indicator changes state under sustained overload; nothing in the readout blocks or locks against the audio path.
- [ ] The project name shows in the bar with an unsaved-changes marker that appears on the first Action and clears on save.
- [ ] Story 10, worked: launch with no prior project, arm the seeded audio track, press R, play a few seconds of input, press R again — a recorded clip appears on that track, its audio file is inside the untitled project's audio folder, and no dialog appeared at any point.
- [ ] Every bare-letter key of this slice types its character instead of acting while the project-name field or the BPM field has focus.
