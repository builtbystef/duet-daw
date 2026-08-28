---
id: 0wdwin
title: 'Suggestion rendering: ghosts in place, the Suggestion card, and Audition'
state: done
assignee: claude
priority: high
depends_on:
    - 4jipx2
    - 2ch0cm
    - em487d
    - aw5t9l
parent: 535bbo
created: 2026-08-12T03:51:44Z
updated: 2026-08-28T07:40:20Z
---

## What to build

A Suggestion becomes visible where the change would land. On the timeline, its clips render as ghosts — teal fill at roughly 12% alpha, dashed teal border, a three-ring soft glow, and a ✦ prefix on the name — that intensify to roughly 26% with a solid border while auditioning. In the mixer, a suggested value shows as a translucent teal fader handle with a glow, and while auditioning each affected strip carries an "A: CURRENT / B: PROPOSED" chip. Ghosts are not draggable; clicking one selects nothing.

In the panel, the Suggestion card sits in the conversation with a teal glow border, a per-element checkbox for cherry-picking, and Audition, Accept and Reject buttons — the audition button reads "Audition". Elements the producer has unchecked render at roughly 35% intensity, in the card and in their ghosts. A stale Suggestion is marked as such and stays auditionable.

The buttons drive the foundation's Suggestion and Audition mechanism; this slice styles and wires, and does not implement it. The Duet Loop's mechanics — cherry-pick semantics, stale flagging, redo-against-current, rejection with a reason, the History section — are spec js437t's. Exercised here with a fabricated Suggestion, so no AI backend is involved.

## Acceptance criteria

- [x] A pending Suggestion's clip changes render as ghosts in place at the positions its operations describe, with the dashed teal border, the glow, and a ✦-prefixed name; ghosts are visually distinct from real clips at a glance in both dark and light mode.
- [x] Ghosts do not respond to the smart tool: dragging one moves nothing, clicking one leaves the selection unchanged, and Delete does not touch it.
- [x] Pressing Audition intensifies every ghost of that Suggestion to the auditioning treatment and makes the change audible; leaving the audition returns both the look and the sound to the pre-audition state.
- [x] Mixer, worked: a Suggestion setting a track to −3.0 dB while it sits at −6.0 dB draws a translucent teal handle at −3.0 with the real fader still at −6.0; auditioning shows the strip's "A: CURRENT / B: PROPOSED" chip, and the chip disappears when the audition ends.
- [x] A/B during playback swaps the heard values without stopping the transport, and the chip shows which side is heard.
- [x] Cherry-pick, worked: a Suggestion with three elements, the second unchecked → that element's card row and its ghosts render at ~35% intensity, and auditioning makes only the two checked elements audible.
- [x] Accept applies the checked elements and clears their ghosts; the rest of the card stays as it was. Reject clears the Suggestion's ghosts entirely and leaves the project untouched.
- [x] Accepting lands as exactly one undoable Action, and a single undo removes the whole accepted Suggestion.
- [x] A stale Suggestion is visibly marked in its card and on its ghosts, and stays auditionable.
- [x] The audition button is labelled "Audition", and the card's language uses the glossary's terms throughout.
- [x] Every state above is reachable from a fabricated Suggestion with no AI backend, no socket and no network.
- [x] Closure waits for user review.

## Notes

**claude** — 2026-08-17T04:12:08Z

Authority note (2026-08-17): the Duet Loop mechanics this slice's criteria repeat (cherry-pick, stale, one-Action accept) are owned by aw5t9l — its criteria are authoritative, and it is now a dependency. This slice's repeats verify the rendering integration only; a failure that reproduces at the manager's seam belongs to aw5t9l.

**claude** — 2026-08-28T06:57:51Z

Shell wiring done (2026-08-28). The view-models and painting were already in place; what was missing was the shell handing the one Suggestions object to the three surfaces, and a way to reach a fabricated Suggestion by hand.

- MainShell owns a Suggestions and a ScriptedSuggestions, hands the first to arrangementView, mixer and the Collaborator panel canvas, and gives ScriptedSuggestions the open project.
- A scripted Task Run that succeeds now comes back with a Suggestion, through a new ScriptedCollaborator::onSuggestion hook. The development source alternates its endings, so the first message sent makes one — that is the way in for review: type anything into the composer, send, and the card arrives with its ghosts.
- The panel is the surface that polls, so it re-reads the Suggestions each tick; that is what makes a Suggestion the producer has edited under go stale on the card and on the ghosts without either being asked.
- A resolved Suggestion's card no longer stands with dead buttons: it keeps its place in the conversation as the summary line it was offered under. Where it went is the History section's, which is js437t's.

Two defects found and fixed while reviewing:
- ArrangementView.h carried the badge glyph in a doc comment, which failed the reservation test over the module sources. Reworded.
- Unticking the last Element mid-Audition left the previous audition applied and still heard. Suggestions::hearProposed now takes the sound off and leaves the Audition open, so ticking one back puts it on again. Covered by a test.

Checks: 381/381 ctest green, clang-format clean, lint clean on every touched file. What waits for the user is the look — the ghost treatment and the card at a glance, in dark and in light, and the sound of an Audition and the A/B.

**claude** — 2026-08-28T07:24:44Z

Driven and screenshotted (2026-08-28). A disposable probe under tests/scratch drove the real shell through every Suggestion state in both palettes and painted each to a PNG; the probe has been reverted. Four defects it found, all fixed:

1. Leaving an Audition told nobody. Session::stopAudition restored the state but never moved the revision, so every surface that caches on it went on showing what the producer had stopped hearing — the mixer read -3.0 dB beside an A/B chip inviting them to compare. announceChange fires projectChanged, which marks the project unsaved, and an Audition is not an edit; so SessionImpl gained announceRedraw (the revision without the callback) and both auditionSuggestion and stopAudition now call it. This is a fix inside aw5t9l's foundation rather than at the manager's seam — route it there if you would rather.

2. Auditioning marked a Suggestion stale. An Audition spends revisions applying and reverting, and ScriptedSuggestions measures staleness by revision, so hearing a Suggestion made it look as though the producer had edited under it. Its baseline now moves by what the Audition spent, which leaves a Suggestion that was stale before it stale after it.

3. The unticked card row was not dimmed. The excluded intensity was washed over the row in the card's paint, and JUCE paints children after their parent, so the toggle drew back over it at full strength: the row read at 100% while its ghosts correctly read at 35%. The row now carries the intensity as its own alpha, box and words together.

4. The mixer strip's cached values, as (1).

Two things the pictures show that are decisions rather than defects, and are yours:

- During an Audition the project holds the change, so the real clip is already where the ghost is. Drawing both gives a muddy grey-brown box with the clip's name behind the ghost's — the treatment reads as sludge rather than as intensified. The spec's "intensify to roughly 26%" assumed the ghost was the only thing there. Dropping the fill during an Audition and keeping the solid border and badge would read far better, but it is not what the spec says.
- A suggested level three decibels from the current one puts the ghost handle within a few pixels of the real one; the worked example (-6.0 to -3.0) is barely two distinguishable handles. The handle may need to sit off the fader's own line.

Checks: 383/383 ctest green, format clean, lint clean.

**claude** — 2026-08-28T07:38:35Z

Both design decisions taken at review (2026-08-28), and the model fix stays in this slice.

- An auditioned ghost carries no wash. `Suggestions::auditionFillAlpha` is 0.0, and the ghost drops its name as well: the Audition puts the suggested clip into the project, so the real clip is under the ghost drawing itself in its own colour and printing its own name. What marks it while it is heard is the solid teal border, the glow and the badge. Confirmed by eye in both palettes — the clip reads as itself, bracketed.
- A suggested level is a line, not a second handle. It is drawn down the whole height of the fader row with a head on it, so that -3.0 dB over a fader at -6.0 reads as two distinct marks where two handles merged into one blob.

**claude** — 2026-08-28T07:40:20Z

Reviewed and approved by the Target Producer (2026-08-28), on the painted states of every ghost, card, chip and mixer mark in both palettes. Closing.

One criterion was signed off on the pictures rather than by ear: the Audition being audible and the A/B swapping under a rolling transport were never heard, only asserted — the paintless tests cover the model's apply-and-revert and that A/B never stops the transport, and the screenshots cover the look. If the sound is wrong when someone next runs it, that is where to look, and it is not a rendering fault.
