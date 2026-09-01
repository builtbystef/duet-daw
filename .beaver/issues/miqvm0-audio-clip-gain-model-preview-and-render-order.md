---
id: miqvm0
title: Audio clip gain model, preview, and render order
state: todo
priority: low
labels:
    - session:task
    - roadmap:h0eir5
depends_on:
    - jpv27l
parent: stoai7
created: 2026-09-01T18:40:08Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Expose Tracktion audio-clip gain in producer units and add the transient preview needed by a direct gesture.

## Settled contract

- `ClipInfo::gainDb` is audio-only, defaults 0 dB, and clamps to -60..+24 dB. MIDI writes are rejected.
- `EditOps::setClipGainDb` is the durable write. `Session::previewClipGainDb` is audible but undo/dirty neutral; gesture completion restores preview before one `Set Clip Gain` Action.
- Gain is non-destructive and pre-insert/pre-fader/pre-send. It combines multiplicatively with fades; source bytes never change.
- Copy/duplicate/split preserve gain; save/reopen and Save As read it exactly.

## Acceptance and tests

- [ ] Set/clamp/copy/split/persist/undo behavior is digest-exact through public seams.
- [ ] Offline features measure -12, 0, and +12 dB deltas within tolerance before track processing and through a post-fader send.
- [ ] Preview/cancel writes no Action and source hashes remain byte-identical.

Start in `Session.h/.cpp`, `EditOps.cpp`, and clip/audio tests. Run all AGENTS.md checks before closing.
