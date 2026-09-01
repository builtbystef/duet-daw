---
id: 808ncc
title: Create and present Group tracks
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
depends_on:
    - rog54z
parent: jsfhhg
created: 2026-09-01T18:36:20Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Expose existing `TrackKind::group` creation and its settled restrictions in the arrangement and Mixer. Sends and sidechains are separate tasks.

## Settled behavior

- `+ Add Track` offers Audio, MIDI, Group in that order. Group creation is one `Add Group Track` Action and defaults to `Group N`.
- Group arrangement rows use the track colour with a `GROUP` eyebrow and no clip lane affordance, input, monitor, or record arm. Double-clicking empty group space does nothing; Browser audio/instruments are invalid, effects remain valid.
- Group Mixer strips carry inserts, automation, output, fader, pan, mute, solo, and meters. They read `Bus (sum)` as input and remain visually distinct from Master.
- Model operations reject clip/instrument creation on Group even when called outside the UI; effects and group-to-group cycle-safe output remain valid.

## Acceptance and tests

- [ ] Add, rename, colour, reorder, duplicate, delete, undo, and save/reopen preserve Group kind and restrictions.
- [ ] Arrangement/Mixer component state exposes no dead clip/record/instrument gesture.
- [ ] Browser drop validation and Add Track menu include the exact routes above.

Start in `ArrangementView/Canvas`, `Mixer/Canvas`, Browser validation, and track operation tests. Run all AGENTS.md checks before closing.
