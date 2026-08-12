---
id: cwz0of
title: 'The suggest tool: the edit-operation vocabulary and its validation'
state: todo
priority: medium
depends_on:
    - v5yhh1
    - em487d
parent: js437t
created: 2026-08-12T04:02:52Z
updated: 2026-08-12T04:02:52Z
---

## What to build

The Collaborator's one write-tool, and the closure principle behind it. A call carries a summary and an ordered list of elements; an element is one human-meaningful change carrying one or more edit operations, and it is the cherry-pick unit, so elements must be independently applicable. The operation set mirrors exactly what the Target Producer can do through the milestone-one UI — MIDI notes, clip lifecycle and placement, tracks and routing, mixer values and sends, plugins and their parameters and sidechain source, automation points, tempo and time signature — and nothing in it creates audio content.

Validation happens before anything exists: unknown ids, out-of-range values, and malformed operations come back as an error the model can correct and retry against. A valid call creates a Suggestion through the foundation's mechanism and returns its id, altering nothing in the project. A run may produce commentary, a Suggestion, or both, and at most one new Suggestion.

## Acceptance criteria

- [ ] Worked: a call with one element carrying a mixer operation that sets a track to −3.0 dB returns a suggestion id; the created Suggestion carries that summary, that element description, and that operation; the project digest and the undo stack are unchanged.
- [ ] Every operation domain the spec enumerates round-trips into a Suggestion and reads back identically: MIDI notes, clip lifecycle and placement, track lifecycle and routing, mixer values and sends, plugin add, remove, reorder, parameter and sidechain source, automation points, tempo and time signature.
- [ ] The vocabulary contains no operation that creates audio content: the audio-clip operations that exist are limited to moving, trimming, looping, duplicating, and deleting clips that already exist.
- [ ] Validation, worked: an operation naming a clip id that does not exist is rejected with an error naming the offending id and the operation's position; nothing is created; a corrected retry within the same run succeeds.
- [ ] Validation, worked: a mixer operation with a volume outside the permitted range is rejected the same way, while the same operation at a legal boundary value is accepted.
- [ ] Operations inside one element may target what an earlier operation of that same element creates; an element that depends on another element's creation is rejected, because elements must be independently applicable.
- [ ] A second call within one Task Run is refused with an error and the first Suggestion stands.
- [ ] A run that calls no write-tool produces commentary alone, and a run that produces both commentary and a Suggestion is possible.
- [ ] Built-in plugin parameters are set in real units and external plugin parameters in normalized 0..1, and a value in the wrong domain is rejected rather than silently converted.
