---
id: aw5t9l
title: 'The Duet Loop: the Suggestion manager''s state machine'
state: todo
priority: medium
depends_on:
    - cwz0of
parent: js437t
created: 2026-08-12T04:03:08Z
updated: 2026-08-12T04:03:08Z
---

## What to build

The mechanics that make a Suggestion a conversation rather than a one-shot patch. States are pending, then accepted, rejected, or superseded, with an orthogonal stale flag. Several Suggestions can be pending at once, independently. Any producer edit touching an entity a pending Suggestion's operations reference flips that Suggestion stale — still auditionable, never auto-merged, because the producer's own edits always win. Redoing a stale Suggestion against current state resolves it as superseded and starts a fresh Task Run carrying the original request and what changed since.

Cherry-pick: accepting an element applies exactly that element's operations, rejecting one drops it, and the Suggestion resolves when every element has. Replying to a pending Suggestion supersedes it with the revision; replying to a rejected one leaves it rejected and yields a new pending Suggestion; the rejection reason is first-class input to the revision run. Acceptance lands in the shared undo history as exactly one Action. Suggestions and the conversation live in memory for the app session and die with it.

## Acceptance criteria

- [ ] Two Suggestions can be pending at once, and resolving one leaves the other untouched in every respect.
- [ ] Stale, worked: a Suggestion pending against clip X goes stale the moment the producer moves clip X; a Suggestion referencing only clip Y does not.
- [ ] A stale Suggestion stays auditionable and is never applied by any later event; only an explicit acceptance applies anything.
- [ ] Redo against current state resolves the stale Suggestion as superseded and starts exactly one new Task Run whose prompt carries the original request and the intervening change.
- [ ] Cherry-pick, worked: a Suggestion of three elements — accept the first, reject the second → the first's operations are in the project as one Action, the second's are gone, the third is still pending, and the Suggestion is unresolved; resolving the third resolves the Suggestion.
- [ ] Accepting one element lands as exactly one Action: a single undo removes that element's changes and nothing else.
- [ ] Accept-all resolves every element and lands as one Action, not one per element; reject-whole resolves every element and applies nothing.
- [ ] Revision, worked: replying to a pending Suggestion makes it superseded and the reply's run produces a new pending Suggestion; replying to a rejected Suggestion leaves it rejected and produces a new pending one.
- [ ] The typed rejection reason is carried into the revision run's prompt.
- [ ] A resolved Suggestion cannot change state again: accepting a rejected or superseded Suggestion is refused.
- [ ] Rejecting leaves the project digest, the undo depth, and the redo depth exactly as they were before the Suggestion arrived.
- [ ] Suggestions and the conversation are in-memory only: nothing about them reaches the project folder, a save during a pending Suggestion writes a file without it, and both are gone after an app restart.
