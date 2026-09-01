---
id: ie6bjp
title: Clip fades and explicit crossfades
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

Give audio clips editable fade-in and fade-out handles and let two overlapping clips on one track form an explicit crossfade. The edits are non-destructive envelope state on the clips; source files remain untouched.

## Acceptance criteria

- [ ] Every audio clip exposes fade-in and fade-out handles whose pending time and curve are visible while dragged; MIDI clips expose no dead fade handles.
- [ ] A fade drag snaps horizontally with Alt bypass and commits as one Set Clip Fade Action; Escape restores the original with no Action.
- [ ] Fade state survives trim, move, copy, save/reopen, and Save As with a defined clamp when a trim becomes shorter than its fades.
- [ ] Two overlapping audio clips can be linked as an explicit crossfade from a visible route; the overlap and both contributing envelopes are drawn.
- [ ] The default crossfade avoids a level hole for equal correlated material, and changing its boundary/shape is audible and undoable as one Action.
- [ ] Removing a crossfade restores independent clip fades without changing either source file.
- [ ] Offline feature assertions verify silence at a full fade edge, the expected midpoint level, continuity through a crossfade, and digest-exact undo.
- [ ] No automatic crossfade is silently created merely because clips overlap.

## Notes

**agent** — 2026-09-01T18:42:01Z

AFK decomposition: fade model gfpfwl -> fade UI btqhk2; explicit crossfade model qgl6tk plus fade UI -> crossfade UI 0as8g4. Individual curves are Linear/Convex/Concave/S; crossfades are explicit Equal Power (default) or Linear.
