---
id: btqhk2
title: Arrangement fade handles and curve selection
state: todo
priority: low
labels:
    - session:task
    - roadmap:h0eir5
depends_on:
    - gfpfwl
parent: ie6bjp
created: 2026-09-01T18:39:45Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Add direct fade-in/out gestures to audio clips using the existing transient smart-tool pattern.

## Settled interaction

- Selected audio clips show top-left fade-in and top-right fade-out handles, each at least 18x18 logical pixels. MIDI clips show none.
- Horizontal drag sets fade seconds from clip geometry, snaps to grid with Alt bypass, previews the envelope/seconds/curve, and commits one `Set Clip Fade` Action. Escape/focus loss restores with none; double-click resets that fade to zero.
- Right-click a handle chooses Linear, Convex, Concave, or S-curve, committing one Action only when changed. The curve is drawn over the waveform and the pending clamp of the opposite fade is visible during drag.
- Fade handles win hit precedence over loop/trim only in their top-corner target; elsewhere existing smart-tool behavior is unchanged.

## Acceptance and tests

- [ ] Paintless gesture tests cover both edges, snap/Alt, proportional clamp, reset, curve choice, cancel, and hit precedence.
- [ ] Component tests cover audio-only handles, cursor, tooltip/readout, focus, and Action routing without screenshot tests.

Start in `ArrangementView/Canvas` and smart-tool tests. Run all AGENTS.md checks before closing.
