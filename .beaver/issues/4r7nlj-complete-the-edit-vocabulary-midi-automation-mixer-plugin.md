---
id: 4r7nlj
title: 'Complete the edit vocabulary: MIDI, automation, mixer, plugin parameters, transport'
state: todo
priority: high
depends_on:
    - quiwf3
parent: b1j3me
created: 2026-08-11T01:50:39Z
updated: 2026-08-17T04:10:16Z
---

## What to build

The remaining op domains of the vocabulary, so that the operation set covers everything js437t's suggest vocabulary enumerates — one for one: `midi.*`; the rest of `clip.*` (setLoop, duplicate); the rest of `track.*` (create with kind midi/audio/group, setOutput); `mixer.set` (volume, pan, mute, solo) and `mixer.setSend`; `plugin.add`, `plugin.remove`, `plugin.reorder`, `plugin.setParam`, `plugin.setSidechainSource` (against engine built-in plugins — external VST3s are a later slice, aty85a); `automation.setPoints`/`removePoints`; `project.setTempo`/`setTimeSignature`. Transport control also lands here, written with no UndoManager so undo can never stop or reposition the transport. Time crosses the facade as plain seconds/beats doubles; every mutation goes through performAction.

The built-in plugins are the engine-shipped devices (decision recorded on b1j3me, 2026-08-17): the engine's 4OSC and sampler as the two instruments; its EQ, compressor, and reverb as the three effects — surfaced through the facade under those Duet-facing names.

Recording ops (arm, input, monitoring, record start/stop) are not here — they are their own slice, nfjr5x.

## Acceptance criteria

- [ ] MIDI ops: insert MIDI clip; add, remove, move, and resize notes; set velocity. Worked example: `performAction("Add note", ...)` adding a note (pitch 60, start beat 1.0, length 0.5 beats, velocity 100) is one undo step; the note reads back with those values; undo/redo are digest-exact.
- [ ] Clip ops complete: set looped state and loop length; duplicate a clip to a bar on the same or another track. Worked example: duplicate a clip to bar 5 → the copy reads back at bar 5 with its source pinned project-relative; setLoop (looped, 2 bars) reads back; each is one Action with digest-exact undo.
- [ ] Track ops complete: create a track of kind midi, audio, or group (a midi track optionally created with a built-in instrument); set a track's output to a group bus. Worked example: create a group track, route an audio track's output to it → the routing reads back; undo restores the previous routing digest-exactly.
- [ ] Mixer ops: set volume, pan, mute, and solo; set a send level to a bus. Worked example: set volume to −6.0 dB → read-back returns −6.0 dB; a send to a group bus at −12.0 dB reads back exactly; mute and solo read back; each is one Action with digest-exact undo.
- [ ] Plugin-chain ops: add a built-in plugin at a chain position, remove it, reorder it, set a parameter by value, set a sidechain source. Worked example: add a compressor to a track, set its sidechain source to another track, set a parameter → all read back; undo restores the prior chain digest-exactly.
- [ ] Automation ops: add, move, remove automation points. Worked example: a point at 2.0s with value 0.75 reads back exactly; undo restores the prior curve digest-exactly.
- [ ] Project ops: set tempo and time signature as Actions. Worked example: set 128 BPM and 6/8 → both read back exactly; undo restores 120 BPM and 4/4.
- [ ] Transport ops (play, stop, set position) go through the facade with no UndoManager: start playback, reposition, then undo an earlier edit Action — the transport keeps playing and its position is unchanged.
- [ ] Every new op is expressed exactly once, writes through the Edit's UndoManager (transport excepted), and is callable only inside performAction.
- [ ] Manual demo, recorded as a note on this issue: with the transport rolling under pw-jack, each op domain lands audibly with no glitch or xrun (prototype-verified behavior spot-checked in the product app; not an automated test).
