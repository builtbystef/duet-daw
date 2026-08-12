---
id: 0wdwin
title: 'Suggestion rendering: ghosts in place, the Suggestion card, and Audition'
state: todo
priority: high
depends_on:
    - 4jipx2
    - 2ch0cm
    - em487d
parent: 535bbo
created: 2026-08-12T03:51:44Z
updated: 2026-08-12T03:51:44Z
---

## What to build

A Suggestion becomes visible where the change would land. On the timeline, its clips render as ghosts — teal fill at roughly 12% alpha, dashed teal border, a three-ring soft glow, and a ✦ prefix on the name — that intensify to roughly 26% with a solid border while auditioning. In the mixer, a suggested value shows as a translucent teal fader handle with a glow, and while auditioning each affected strip carries an "A: CURRENT / B: PROPOSED" chip. Ghosts are not draggable; clicking one selects nothing.

In the panel, the Suggestion card sits in the conversation with a teal glow border, a per-element checkbox for cherry-picking, and Audition, Accept and Reject buttons — the audition button reads "Audition". Elements the producer has unchecked render at roughly 35% intensity, in the card and in their ghosts. A stale Suggestion is marked as such and stays auditionable.

The buttons drive the foundation's Suggestion and Audition mechanism; this slice styles and wires, and does not implement it. The Duet Loop's mechanics — cherry-pick semantics, stale flagging, redo-against-current, rejection with a reason, the History section — are spec js437t's. Exercised here with a fabricated Suggestion, so no AI backend is involved.

## Acceptance criteria

- [ ] A pending Suggestion's clip changes render as ghosts in place at the positions its operations describe, with the dashed teal border, the glow, and a ✦-prefixed name; ghosts are visually distinct from real clips at a glance in both dark and light mode.
- [ ] Ghosts do not respond to the smart tool: dragging one moves nothing, clicking one leaves the selection unchanged, and Delete does not touch it.
- [ ] Pressing Audition intensifies every ghost of that Suggestion to the auditioning treatment and makes the change audible; leaving the audition returns both the look and the sound to the pre-audition state.
- [ ] Mixer, worked: a Suggestion setting a track to −3.0 dB while it sits at −6.0 dB draws a translucent teal handle at −3.0 with the real fader still at −6.0; auditioning shows the strip's "A: CURRENT / B: PROPOSED" chip, and the chip disappears when the audition ends.
- [ ] A/B during playback swaps the heard values without stopping the transport, and the chip shows which side is heard.
- [ ] Cherry-pick, worked: a Suggestion with three elements, the second unchecked → that element's card row and its ghosts render at ~35% intensity, and auditioning makes only the two checked elements audible.
- [ ] Accept applies the checked elements and clears their ghosts; the rest of the card stays as it was. Reject clears the Suggestion's ghosts entirely and leaves the project untouched.
- [ ] Accepting lands as exactly one undoable Action, and a single undo removes the whole accepted Suggestion.
- [ ] A stale Suggestion is visibly marked in its card and on its ghosts, and stays auditionable.
- [ ] The audition button is labelled "Audition", and the card's language uses the glossary's terms throughout.
- [ ] Every state above is reachable from a fabricated Suggestion with no AI backend, no socket and no network.
- [ ] Closure waits for user review.
