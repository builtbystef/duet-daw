---
id: wi3f34
title: Selected-clip gain badge and direct gesture
state: todo
priority: low
labels:
    - session:task
    - roadmap:h0eir5
depends_on:
    - miqvm0
parent: stoai7
created: 2026-09-01T18:40:08Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Give each selected audio clip a readable gain control without adding a new inspector pane.

## Settled interaction

- A selected audio clip shows a top-centre badge such as `Gain 0.0 dB`; MIDI shows none. Badge tooltip states `Clip gain — before track effects`.
- Drag vertically: upward raises gain, 4 logical pixels per dB; Shift is fine mode at 20 px per dB. Pending value is visible and audibly previewed. Escape/focus loss cancels; mouse-up commits one Action; double-click resets to 0 dB.
- Context menu `Clip Gain…` opens numeric dB entry with exact -60..+24 validation and gives the same one-Action result.
- Waveform drawing scales by `10^(gainDb/20)` within clip bounds; boosted clipping of the drawing is allowed and marked by the numeric badge, never by altering samples.
- Gain badge wins pointer precedence only inside its 18x18-minimum target; fade/loop/trim/body gestures remain unchanged elsewhere.

## Acceptance and tests

- [ ] Paintless tests cover coarse/fine drag, clamp, preview/cancel/commit/reset/numeric entry, waveform scale, and hit precedence.
- [ ] Component tests cover audio-only composition, tooltip, keyboard focus, and cursor without pixels.

Start in `ArrangementView/Canvas` and smart-tool tests. Run all AGENTS.md checks before closing.
