---
id: aw5t9l
title: 'The Duet Loop: the Suggestion manager''s state machine'
state: done
assignee: claude
priority: medium
depends_on:
    - cwz0of
    - em487d
parent: js437t
created: 2026-08-12T04:03:08Z
updated: 2026-08-28T05:44:50Z
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

## Notes

**claude** — 2026-08-28T05:44:47Z

Built as `duet::collab::SuggestionManager` in `duet_collab_tools` (the target that may link the model), over the write-tool's own applicable form and over the model's Audition from em487d. Every acceptance criterion is worked as its own test in `tests/SuggestionManagerTests.cpp`, against a real project and the real `suggest` tool — a Suggestion built by hand in a test would be one nothing could have made.

Decisions taken while building:

- **Staleness is measured, not reported.** Every id a pending Suggestion's operations name is remembered as the text the project reads back for it at the moment the Suggestion was made; a Suggestion the project now says something else about is stale. Nothing subscribes to edits. This is what makes the rest fall out for free: an undo back to what the Suggestion was made against takes the staleness with it, applying a Suggestion's own Element rebaselines it rather than staling it, and another Suggestion's acceptance is the producer editing and does stale it. `Session::revision()` is the cheap guard, so an untouched project costs one comparison rather than a walk; a live Audition suspends the comparison entirely, because the project then holds a Suggestion's changes rather than the producer's.
- **Which ids are found is a walk over an operation's values, not a list of field names.** An operation was already accepted by `SuggestTool`, so any string in it that parses as a project id is one it touches; a field list would silently miss a field added later. Tempo and time signature have no id, so `project.*` operations key on `project`; an automation curve is named by what it drives, so its key is written out of that.
- **Cherry-pick is `model::Suggestion::append`.** Accepting whatever is left of a Suggestion appends those Elements' operation lists into one, renumbering the appended placeholders past everything the first list handed out, so neither list can resolve the other's creations. That is the whole of why any set of independently applicable Elements is still one Action. Accepting one Element applies that Element's own list, named for the Element; accepting the rest applies the appended list, named for the summary.
- **Prompts put the request first and the change last**, which is the prompt-cache discipline the spec asks of everything this side sends: a redo carries the original request, then each named thing that has changed, what it was and what it is now.
- **Auditioning belongs here and there is one at a time**, over `Session::auditionSuggestion`: what it hands the model is whatever the Suggestion has left to accept, so what a producer hears is what accepting it now would do.

Facts a reviewer needs: `SuggestionManager` is message-thread-only and holds a `Session&` that must outlive it. Rebaselining reads the whole Suggestion's references, including Elements already resolved, so a Suggestion can go stale over an id only a rejected Element named — the spec binds staleness to the Suggestion's operations, and that reading is the conservative one. `ARCHITECTURE.md` gains the manager's paragraph under the Collaborator service, and `duet_model`'s paragraph gains `append`.

Checks: format clean, `./scripts/lint.sh` full sweep exit 0 with no diagnostics, 362/362 ctest. (The build tree had lost `tests/CTestTestfile.cmake` and reported "No tests were found"; a reconfigure restored discovery — nothing to do with this change.)
