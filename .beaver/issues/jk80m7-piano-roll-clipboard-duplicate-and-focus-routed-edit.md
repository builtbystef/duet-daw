---
id: jk80m7
title: Piano Roll clipboard, duplicate, and focus-routed edit commands
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
parent: kmsbfq
created: 2026-09-01T18:37:38Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Add an internal note clipboard to `PianoRoll` and route standard edit keys to the visibly focused arrangement or Piano Roll. Add the visible Quantize command to the Piano Roll strip.

## Settled command policy

- Ctrl+C copies selected notes relative to the earliest selected start; no Action. Ctrl+X copies then removes them as one `Cut Notes` Action. Clipboard lives for the app session and survives opening another MIDI clip/project only as value data, never refs.
- Ctrl+V places the earliest copied note at `max(0, playheadBeat - openClipTimelineStartBeat)`, preserves relative starts/pitch/length/velocity, extends the MIDI clip to the latest pasted note when needed, selects new notes, and is one `Paste Notes` Action.
- Ctrl+D duplicates in place plus `ceil(selection span / grid) * grid`; span is latest end minus earliest start and minimum one grid. It extends the clip if needed and is one `Duplicate Notes` Action.
- Empty/invalid/open-no-clip cases do nothing. Ctrl+X/C/V/D route to Piano Roll only while it or its command controls own focus; arrangement otherwise retains clip commands. Hidden surfaces never receive them and text fields keep native clipboard behavior.
- A labelled `Quantize <grid>` button invokes the existing one-Action quantize and updates with grid selection.

## Acceptance and tests

- [ ] Worked literal note sets prove copy/cut/paste/duplicate values, extension, selection, Action names, and one-step undo.
- [ ] Focus dispatch tests cover arrangement, Piano Roll, hidden bottom panel, and active text editor.
- [ ] Quantize button/shortcut state reflects the active grid and is keyboard reachable.

Start in `PianoRoll`, `Shortcuts`, `MainShell`, `PianoRollCanvas`, and focused tests. Run all AGENTS.md checks before closing.
