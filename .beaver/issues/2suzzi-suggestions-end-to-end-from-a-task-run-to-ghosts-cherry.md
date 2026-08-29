---
id: 2suzzi
title: 'Suggestions end to end: from a Task Run to ghosts, cherry-pick, revision, stale'
state: in-progress
priority: medium
labels:
    - needs-review
depends_on:
    - aw5t9l
    - 7tw2tz
    - 0wdwin
parent: js437t
created: 2026-08-12T04:03:58Z
updated: 2026-08-29T00:07:57Z
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

## Notes

**claude** — 2026-08-17T04:12:08Z

Authority note (2026-08-17): the mechanics criteria repeated here are integration checks. aw5t9l owns the state machine, em487d the Audition mechanism, 0wdwin the rendering; a failure here that reproduces at one of those seams belongs to that ticket.

**claude** — 2026-08-29T00:07:57Z

The Duet Loop is on real Task Runs. What was missing was never the mechanism —
aw5t9l built the state machine, em487d the Audition, 0wdwin the rendering — but
the three of them meeting a run: `duet::app::SuggestionSurfaces` is the piece
that was not there, and `Collaborator` now owns the open project's
`SuggestionManager` and its `suggest` tool.

Seams used, both named by the spec. The socket protocol
(`tests/CollaboratorTests.cpp`), where a real service, a real socket and the
test-double sidecar as a real child process make a Suggestion and the panel and
the surfaces read it; and the manager's own C++ interface, one step out at the
adapter (`tests/SuggestionSurfacesTests.cpp`), where the ghosts, the
cherry-pick, the Audition and the staleness are worked against a real project
and the real write-tool. `tests/CollaboratorLoopLiveTests.cpp` is the whole loop
end to end against the real sidecar.

Against the criteria:

- A real run's `suggest` call becomes a card where the producer asked for it and
  ghosts where its operations land, with the project digest and the undo depth
  exactly as the run found them.
- Audition is heard in place and leaves the project digest-exact; A/B swaps the
  heard values with the transport rolling and never stops it.
- Cherry-pick, worked: three Elements with the second unticked — the audition
  hears the first and third, Accept applies those two as one Action a single
  undo removes entirely, and the second stays on the card.
- Reject with a typed reason starts one run carrying that reason and the
  original request, and the revision replaces the rejected card where it stood;
  the conversation ends the turn with one card, not two.
- Stale, worked: moving the clip a pending Suggestion names marks the card and
  its ghosts, and it stays auditionable with nothing merged.
- The redo control resolves the stale Suggestion and starts one run carrying the
  request and every named thing that has changed, was and is now; the answer is
  a fresh card in its place.
- Two Suggestions pending at once draw their marks on the strips they are about
  and resolve independently.
- Accepting from the card is one Action; rejecting leaves both histories alone.
- A Suggestion arriving with the transport rolling does not interrupt playback.

Decisions made:

- **Reject with a reason is a reply, not a rejection.** The manager reads a
  Suggestion any Element of which was accepted as accepted, so rejecting the
  remainder of a cherry-picked one and *then* replying is refused — the loop
  died there the first time it was run end to end. Saying why is asking for a
  better one, so the reason goes through `reply`, which supersedes the pending
  Suggestion and starts the run; Reject with nothing typed is the plain
  rejection it reads as. The History therefore says "asked again" for a
  rejection with a reason, which is what happened to it.
- **A revision takes the place of the card it revises.**
  `CollaboratorPanel::showSuggestion` now takes what the new Suggestion revises,
  which the manager already tracks, and replaces that entry where it stands. A
  conversation of several turns holds one card rather than a pile.
- **The manager gained a subset accept and a subset audition.**
  `accept (id, elements)` and `audition (id, elements)` — the same acts narrowed
  to a list, named for the Element when it names one and for the summary when it
  names more. Nothing else could make "accept the two the producer left ticked
  as one Action, and leave the third pending" true, and em487d's boundary note
  said cherry-pick at that seam is applying a sub-list.
- **Ghosts are read off the operations.** The adapter walks the `suggest` call's
  own operations against the project as it stands: `clip.createMidi`,
  `clip.move`, `clip.trim` and `clip.duplicate` draw a clip, `mixer.set` with a
  level draws a fader mark, and an operation with no picture of its own — a note
  edit, a plugin, a deletion — is applied by an acceptance all the same and read
  as the Element's own words. A card counts only the Elements still pending, so
  the row the producer ticks is mapped back onto the manager's numbering in one
  place.
- **`ScriptedSuggestions` is gone**, with `MainShell::developmentSuggestions()`.
  `MainShell::pendingSuggestions()` is where the real source arrives, the same
  arrangement the panel's own source has. What it was covering moved onto the
  real manager in `tests/SuggestionSurfacesTests.cpp`; the one component case
  that needed a source keeps a double local to that suite, which links the
  interface and nothing under it.
- **The card grew two controls**: a box for the rejection reason, so that saying
  why travels with the rejection rather than being typed somewhere else, and the
  redo control, which a Suggestion carries only while the project has moved
  under it.

Facts a reviewer needs:

- Every run of a Collaborator now starts in one place, `Collaborator::startRun`,
  whether the producer asked for it or a card did — which is what puts the Task
  Run card on screen for a revision and a redo.
- The `suggest` tool is registered per project like the read tools, so a project
  swap takes it with the rest; it carries the same exposure 9tdwdq names and no
  new one.
- `tests/sidecar_double` learned two things: `call-tools` takes a list of lists,
  one per run, so a conversation of several turns can be answered differently
  each turn, and it now reports `run.start` like the `run-*` scripts, so the
  prompt a revision carries is assertable.

**Criterion 10 is what is left, and it needs you.** The case is written and
runs — `tests/CollaboratorLoopLiveTests.cpp`, request, Suggestion, Audition,
cherry-pick, revision, staleness, redo and History, printing every turn — but a
real provider costs money, needs credentials and reaches the network, so it was
exercised here against the real sidecar with a scripted model instead:

    DUET_LIVE_MODEL=duet-offline:scripted ./build/tests/Debug/duet_tests \
        "the whole Duet Loop runs against a real backend on a fixture project"

The criterion asks for the same case with a provider behind it, and its printed
output recorded here:

    DUET_LIVE_MODEL=openai:gpt-5.6 ./build/tests/Debug/duet_tests "[live]"

Checks: clang-format clean, full `./scripts/lint.sh` sweep exit 0 with no
diagnostics, full Debug build clean, and 473/473 CTest entries pass.

**This issue's closure waits for your review.** Close it to approve, or note the
changes you want and remove the `needs-review` label.
