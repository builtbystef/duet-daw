---
id: 4r7nlj
title: 'Complete the edit vocabulary: MIDI, automation, mixer, plugin parameters, transport'
state: todo
priority: high
depends_on:
    - quiwf3
parent: b1j3me
created: 2026-08-11T01:50:39Z
updated: 2026-08-11T01:50:39Z
---

## What to build

The remaining op domains of the vocabulary, so that the operation set covers everything js437t's propose vocabulary enumerates: MIDI notes, automation points, mixer values, and plugin parameters (against engine built-in plugins — external VST3s are a later slice). Transport control also lands here, written with no UndoManager so undo can never stop or reposition the transport. Time crosses the facade as plain seconds/beats doubles; every mutation goes through performAction.

## Acceptance criteria

- [ ] MIDI ops: insert MIDI clip; add, remove, move, and resize notes; set velocity. Worked example: `performAction("Add note", ...)` adding a note (pitch 60, start beat 1.0, length 0.5 beats, velocity 100) is one undo step; the note reads back with those values; undo/redo are digest-exact.
- [ ] Automation ops: add, move, remove automation points. Worked example: a point at 2.0s with value 0.75 reads back exactly; undo restores the prior curve digest-exactly.
- [ ] Mixer ops: set track volume and pan. Worked example: set volume to −6.0 dB → read-back returns −6.0 dB; undo restores the previous value.
- [ ] Plugin parameter ops: set a parameter on an engine built-in plugin by value; read-back matches; one Action, one undo step, digest-exact undo.
- [ ] Transport ops (play, stop, set position) go through the facade with no UndoManager: start playback, reposition, then undo an earlier edit Action — the transport keeps playing and its position is unchanged.
- [ ] Every new op is expressed exactly once, writes through the Edit's UndoManager (transport excepted), and is callable only inside performAction.
- [ ] Manual demo, recorded as a note on this issue: with the transport rolling under pw-jack, each op domain lands audibly with no glitch or xrun (prototype-verified behavior spot-checked in the product app; not an automated test).
