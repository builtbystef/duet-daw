---
id: em487d
title: 'Suggestion and Audition: apply-and-revert, A/B, one-step accept, zero-trace reject'
state: done
assignee: agent
priority: high
depends_on:
    - 4r7nlj
    - 3vwusn
parent: b1j3me
created: 2026-08-11T01:51:30Z
updated: 2026-08-24T10:04:40Z
---

## What to build

The Suggestion mechanism the Collaborator spec (js437t) depends on, per spec b1j3me / ADR 0004. A Suggestion is data — an ordered op list over the vocabulary, with placeholder refs for items the Suggestion itself creates — until accepted. Audition is apply-and-revert on the real Edit with no UndoManager, so it is invisible to producer undo; A/B is that same apply/revert as a toggle. Accepting first reverts the Audition, then re-applies the ops through one performAction, so an accepted Suggestion is exactly one named undo step. Rejecting discards data. Save interplay: a manual save auto-reverts a live Audition first; the autosave timer skips its tick while an Audition is live; pending Suggestions are never saved and never block saving.

## Acceptance criteria

- [ ] A Suggestion whose op 2 targets the track created by its op 1 (placeholder ref) applies correctly on Audition and on accept.
- [ ] Worked example (spec): entering Audition, the audible/model state contains the Suggestion's changes; leaving it, the canonicalized digest equals the pre-Audition digest; the project file on disk is byte-unchanged throughout.
- [ ] A/B toggling five times ends digest-exact, and no producer undo step ever appears from any Audition apply or revert.
- [ ] Accept → exactly one new undo step carrying the Suggestion's name; a single undo removes every change of the Suggestion, digest-exact; redo restores it.
- [ ] Reject (from idle or from live Audition) → digest, undo-stack depth, and redo-stack depth all equal their pre-Suggestion values.
- [ ] Manual save during a live Audition auto-reverts it first: the saved file contains no Suggestion changes.
- [ ] The autosave timer skips its tick while an Audition is live and resumes after.
- [ ] A pending (unaccepted) Suggestion is absent from the saved file and does not block explicit save.
- [ ] Audition works while the transport rolls (enter, A/B, and revert during playback without stopping it).

## Notes

**claude** — 2026-08-17T04:12:08Z

Boundary clarification (2026-08-17): this slice's Suggestion stays a flat ordered op list with placeholder refs — do not add element semantics here. Element grouping (the cherry-pick unit, js437t) is the Suggestion manager's layer (aw5t9l): the manager holds elements and hands this mechanism the subset of ops to apply. Cherry-pick at this seam is 'apply a sub-list', exactly as skb4tp proved.

**agent** — 2026-08-24T10:04:40Z

Implemented Suggestion and Audition at the model/persistence facade seams. Suggestion is now an engine-free flat ordered operation list covering the edit vocabulary, with placeholders for tracks, clips, notes, and plugins created earlier in the list. Audition materializes that data on a reusable detached Edit sharing the session's Engine, applies/reverts the resulting state on the real Edit with null UndoManager writes, preserves the transport, and isolates/removes engine bookkeeping transactions so existing undo and redo remain exact. Accept reverts B first and replays the list through one performAction named for the Suggestion; reject is data disposal and reverts when live. Explicit save now reverts a live Audition, and elapsed autosave ticks are consumed without writing while B is live. Pending Suggestions remain caller-owned memory and never enter project files. Added facade coverage for placeholder chains, digest and disk exactness, five A/B toggles, undo/redo exactness, idle/live reject, explicit save, autosave, pending save, and rolling transport. Recorded the detached-state architecture and two new engine facts in the project docs. Checks: configure and full Debug build clean; format clean; full lint clean; 198/198 tests passed with 8 existing hardware-dependent skips.
