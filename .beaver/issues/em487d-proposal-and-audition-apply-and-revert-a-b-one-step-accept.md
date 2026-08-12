---
id: em487d
title: 'Proposal and Audition: apply-and-revert, A/B, one-step accept, zero-trace reject'
state: todo
priority: high
depends_on:
    - 4r7nlj
    - 3vwusn
parent: b1j3me
created: 2026-08-11T01:51:30Z
updated: 2026-08-11T01:51:30Z
---

## What to build

The Proposal mechanism the Collaborator spec (js437t) depends on, per spec b1j3me / ADR 0004. A Proposal is data — an ordered op list over the vocabulary, with placeholder refs for items the Proposal itself creates — until accepted. Audition is apply-and-revert on the real Edit with no UndoManager, so it is invisible to producer undo; A/B is that same apply/revert as a toggle. Accepting first reverts the Audition, then re-applies the ops through one performAction, so an accepted Proposal is exactly one named undo step. Rejecting discards data. Save interplay: a manual save auto-reverts a live Audition first; the autosave timer skips its tick while an Audition is live; pending Proposals are never saved and never block saving.

## Acceptance criteria

- [ ] A Proposal whose op 2 targets the track created by its op 1 (placeholder ref) applies correctly on Audition and on accept.
- [ ] Worked example (spec): entering Audition, the audible/model state contains the Proposal's changes; leaving it, the canonicalized digest equals the pre-Audition digest; the project file on disk is byte-unchanged throughout.
- [ ] A/B toggling five times ends digest-exact, and no producer undo step ever appears from any Audition apply or revert.
- [ ] Accept → exactly one new undo step carrying the Proposal's name; a single undo removes every change of the Proposal, digest-exact; redo restores it.
- [ ] Reject (from idle or from live Audition) → digest, undo-stack depth, and redo-stack depth all equal their pre-Proposal values.
- [ ] Manual save during a live Audition auto-reverts it first: the saved file contains no Proposal changes.
- [ ] The autosave timer skips its tick while an Audition is live and resumes after.
- [ ] A pending (unaccepted) Proposal is absent from the saved file and does not block explicit save.
- [ ] Audition works while the transport rolls (enter, A/B, and revert during playback without stopping it).
