---
id: jpv27l
title: 'Release gate: make, record, mix, reopen, and export one eight-bar piece'
state: todo
priority: high
labels:
    - review
    - roadmap:yfpnps
depends_on:
    - kkclj0
parent: yfpnps
created: 2026-09-01T18:08:28Z
updated: 2026-09-01T18:41:15Z
---

## What to prove

Run the usability specification's complete workflow through the shipping application on the development machine. This is the gate for calling the milestone-one interface usable; it integrates the seven slices and does not substitute a fixture or direct API for any producer gesture.

## Preconditions

- Start from a fresh app-global test home or document every retained setting.
- Launch the Debug application through `pw-jack` with real audio output, one real audio input, and one real MIDI input.
- Use only visible shipping controls. Development traces may be observed for diagnosis but may not perform a step.

## Acceptance criteria

- [ ] Fresh launch creates an untitled project and no setup/error surface blocks non-Collaborator work.
- [ ] Set an eight-bar loop from the ruler, enable it, and hear playback wrap at the chosen boundaries.
- [ ] Create a MIDI track with 4OSC, open its built-in editor, shape a clearly distinguishable sound, create/edit an eight-bar part, and use Piano Roll clipboard plus note audition.
- [ ] Audition at least two source samples before choosing one, import it from outside the configured Browser roots, load project-owned audio into the Sampler, and make a playable sample part.
- [ ] Choose a hardware MIDI input, arm, record a MIDI take, stop, and edit the landed notes.
- [ ] Choose a non-default audio input and While Armed monitoring, arm, record a live audio take, hear monitoring while it rolls, and stop with one take on the intended track.
- [ ] Create a Reverb group, insert the built-in Reverb, and make an audible post-fader send to it.
- [ ] Add a compressor to another track, choose a sidechain source, and hear or measure the keyed gain reduction.
- [ ] Mix through the visible fader, pan, mute/solo, send, routing, insert, and Master controls without an operation available only through the Collaborator.
- [ ] Save, close, and reopen. Tracks, clips, notes, device parameters, Sampler mappings, inputs, monitoring choices, groups, sends, sidechains, loop range, sections, and view state return as specified.
- [ ] Export the eight-bar range. The resulting file is audible, has the expected routing/effects, and its duration is eight bars within one render block.
- [ ] Undo and redo are sampled once in each edit domain used above and each names and reverses one producer-meaningful Action.
- [ ] No crash, hang, stuck note, stopped device, missing file, silent failure, or unexplained control state occurs during the run.
- [ ] Record a concise transcript on this issue: machine/input setup, each step, the saved project folder, export path and measured duration, defects encountered, and the Target Producer's verdict.
- [ ] Closure waits for explicit Target Producer approval.

## Failure policy

A failure is fixed in the slice that owns the seam when it reproduces there. A failure that exists only when slices meet gets a new bounded `session:task` child under `yfpnps` carrying `roadmap:yfpnps`; add it as a dependency of this gate, note the failed step, and release the gate so the AFK selector can resume. The gate itself never becomes an implementation bucket. Rerun from the beginning after any product change; partial success is evidence, not a pass.
