---
id: 3xxk8b
title: Recording count-in
state: todo
priority: low
labels:
    - spec
    - roadmap:h0eir5
depends_on:
    - jpv27l
parent: h0eir5
created: 2026-09-01T18:09:16Z
updated: 2026-09-01T18:42:01Z
---

## What to build

Let the Target Producer choose no count-in or a 1- or 2-bar metronome count-in before a take. Pressing Record starts the count visibly and audibly; capture begins exactly at the intended project position after it finishes.

The implementation uses Tracktion's native Off/1-bar/2-bar recording pre-roll. A 4-bar choice was dropped during implementation preparation: Tracktion exposes no such mode, and adding a second Duet-authored click scheduler solely for that length would introduce a new real-time audio path without improving the ordinary count-in workflow.

## Acceptance criteria

- [ ] The transport exposes Off/1/2 bars, shows the current choice, and remembers it with the project's transport/view state without adding producer undo.
- [ ] Starting a take with count-in leaves the playhead/capture start at the intended punch point, plays the metronome for exactly the chosen preceding bars, then arms capture without a second Record press.
- [ ] A visible countdown names the remaining bars/beats and distinguishes counting from recording; the record control changes state at the capture boundary.
- [ ] Stop or Escape during count-in cancels cleanly, creates no take and no Record Take Action, and leaves armed/input/monitoring choices intact.
- [ ] MIDI and audio takes land at the intended beat with existing latency compensation; count-in audio is never recorded into the take or export.
- [ ] Tempo and metre changes determine count-in duration from the project map, including non-4/4 metres.
- [ ] Tests drive the count with the commandable transport/device seam and no wall-clock sleeps; one live review confirms the metronome and boundary feel correct through `pw-jack`.
- [ ] Count-in remains separate from punch-in, pre-roll playback, and loop recording.

## Notes

**agent** — 2026-09-01T18:42:01Z

AFK decomposition: native Off/1/2-bar model and phase seam 04qcp0 -> transport UI nvdslx; physical-device approval is review lkq8fn. The proposed 4-bar option is withdrawn for the reason recorded in the body.
