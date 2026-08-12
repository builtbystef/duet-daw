---
id: 2suzzi
title: 'Suggestions end to end: from a Task Run to ghosts, cherry-pick, revision, stale'
state: todo
priority: medium
depends_on:
    - aw5t9l
    - 7tw2tz
    - 0wdwin
parent: js437t
created: 2026-08-12T04:03:58Z
updated: 2026-08-12T04:03:58Z
---

## What to build

The Duet Loop as the Target Producer experiences it, on real Task Runs. A run that produces a Suggestion renders it as a card in the conversation and as ghosts where the change would land — clips on the timeline, values in the mixer. Audition makes it audible in context; A/B compares current against suggested during playback. Accept applies it as one Action; the per-element checkboxes cherry-pick; Reject with a typed reason produces a revision that supersedes the card in place. A producer edit touching a pending Suggestion's target marks it stale in the card and on its ghosts, and the redo control resolves it and starts a fresh run against current state.

This slice wires the rendering and the manager to real runs; it implements neither.

## Acceptance criteria

- [ ] A real run's Suggestion appears as a card and as ghosts at the positions its operations describe, with nothing in the project changed and no undo step created.
- [ ] Audition makes the Suggestion audible in place and leaves the project digest-exact when it ends; A/B during playback swaps the heard values without stopping the transport.
- [ ] Cherry-pick, worked: a three-element Suggestion with the second unchecked → auditioning makes only the first and third audible, and Accept applies only those two as one Action that a single undo removes entirely.
- [ ] Reject with a typed reason starts exactly one new Task Run carrying that reason, and the revision supersedes the rejected card in place rather than accumulating cards.
- [ ] Stale, worked: with a Suggestion pending against a clip, moving that clip marks the card and its ghosts stale; the Suggestion stays auditionable and nothing merges by itself.
- [ ] The redo control on a stale Suggestion resolves it and starts one fresh run whose prompt carries the original request and what changed; the result arrives as a new card.
- [ ] Two Suggestions pending at once render their ghosts distinguishably and resolve independently.
- [ ] Accepting from the card lands as exactly one Action; rejecting from the card leaves the undo and redo depths exactly as they were.
- [ ] A Suggestion arriving while the transport rolls does not interrupt playback.
- [ ] The whole loop is exercised against a real backend on a fixture project and recorded as a note: request, Suggestion, audition, cherry-pick, revision, stale, redo.
- [ ] Closure waits for user review.
