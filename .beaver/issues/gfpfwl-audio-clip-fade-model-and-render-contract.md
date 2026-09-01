---
id: gfpfwl
title: Audio clip fade model and render contract
state: todo
priority: low
labels:
    - session:task
    - roadmap:h0eir5
depends_on:
    - jpv27l
parent: ie6bjp
created: 2026-09-01T18:39:45Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Expose Tracktion audio-clip fades through the engine-free model facade and named Actions. Crossfade relationships and UI are separate.

## Settled fade contract

- `ClipInfo` carries fade-in/out seconds and `ClipFadeCurve` (`linear`, `convex`, `concave`, `sCurve`) for audio; MIDI always reports zero/linear and rejects writes.
- New/imported audio clips default to zero-length linear fades—no automatic edge fade.
- `setClipFade(clip, edge, seconds, curve)` clamps seconds to [0, clip length]. If fade-in + fade-out exceeds length, scale both proportionally so their sum equals length. A trim that shortens a clip applies the same proportional clamp in the trim Action.
- Fade edits are non-destructive gain envelopes before clip gain/track processing. Copy/duplicate preserves them; split follows the split specification; source bytes never change.

## Acceptance and tests

- [ ] Read/write/clamp, trim, copy, save/reopen, Save As, and digest-exact undo work through public model seams.
- [ ] Feature assertions verify silence at a full fade edge and independently calculated gain at 25/50/75% for all four curves within tolerance.
- [ ] MIDI/invalid refs write nothing and source file hashes remain unchanged.

Start in `Session.h`, `EditOps.cpp`, and clip/audio feature tests. Run all AGENTS.md checks before closing.
