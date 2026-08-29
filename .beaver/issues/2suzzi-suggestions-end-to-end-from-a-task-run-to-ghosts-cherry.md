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
updated: 2026-08-29T07:02:43Z
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

**claude** — 2026-08-29T06:22:26Z

Criterion 10 was run against a real provider (openai:gpt-5.6-terra) twice on
2026-08-29. It has not passed, and the two failures were different — which is
itself the finding.

Run 1 reached the staleness step, 25/26 assertions. The loop worked: the model
read the measured facts ("nearly all of their measured energy is below 500 Hz"),
suggested lifting the Keys 6 dB and opening the filter to 2.2 kHz, the
cherry-pick applied one of two Elements as one Action, and the revision declined
to brighten further and offered a chorus instead. Staleness then failed, and the
staleness rule was not at fault: the revision was made of `plugin.setParam` on
the Keys synth, whose reference set is one plugin id and nothing else, and the
case's hard-coded edit moved a clip, changed a track fader and the tempo — all
of which miss a plugin's description. The fixture's own comment claimed "every
Suggestion about it must name this track or this clip", which is not true of
this vocabulary. Fixed in the working tree: the staleness step now reads the
first id the revision's operations name and edits that entity — a track's
volume, a clip's start, a plugin's bypass, a note's velocity — asserts the
digest moved, and fails loudly if the revision names nothing. Each Element's
operations are now printed, so a live failure is diagnosable from the transcript.
The scripted path passes with it, 29 assertions.

Run 2 failed at the first turn instead: the model answered with commentary and
called `suggest` at no point. Its advice was sound and grounded in the measured
facts, and js437t says the Collaborator "answers with commentary, a Suggestion,
or both" — so a free-form question ("The keys feel buried. What would you do?")
answered with prose is the spec working, not failing. The case nonetheless
REQUIREs a card on turn one.

The decision this needs, and the reason it is not resolved here: criterion 10 as
written is not reliably satisfiable, because it asks a live model for a
Suggestion the spec does not oblige it to make. Either the request becomes
directive so `suggest` is the expected answer, or the case tolerates a
commentary-only turn and asks again, or the criterion is accepted as a
best-effort manual check and the run recorded as evidence. That is a spec
question, not a mechanical one.

**claude** — 2026-08-29T07:02:43Z

Criterion 10 now passes against a real provider. Run of 2026-08-29,
DUET_LIVE_MODEL=openai:gpt-5.6-terra, 36 assertions in 2 cases, all passing.

What the criterion asked to be recorded, one turn at a time:

Request — "The keys feel buried. Change the mix so they sit forward, and give me
each change on its own so I can take some and leave others."

Suggestion — "Bring the Keys forward with independent level and tone options",
two Elements: raise the Keys fader 3 dB (one ghost fader), and open the synth
filter 440 Hz to 1.6 kHz. Grounded in what it read, not guessed: "I found the
Keys are at -12 dB, with the synth filter closed down at 440 Hz."
  {"op":"mixer.set","trackId":"track-1010","volumeDb":-9}
  {"op":"plugin.setParam","pluginId":"plugin-1013","paramId":"filterFreq","value":1600}

Audition — heard in place, digest 1c51e6daf041c5d1, and A/B restored it exactly.

Cherry-pick — 1 of 2 Elements applied, as one Action named for the Element.

Revision — "Give the Keys a wider, more forward position": chorusMix and
ampAttack, both `plugin.setParam` on plugin-1013, replacing the card in place.

Staleness — the case edits what the revision names, which here was a plugin and
nothing else. Bypassing plugin-1013 marked the card stale and left it
auditionable.

Redo — and this is the turn worth reading. The model saw the state the staleness
edit had left: "The 4OSC instrument is currently bypassed, so the Keys cannot
render at all", and offered to restore the instrument and then, separately, lift
the Keys from -9 dB to -6 dB. The redo carried what had changed and the answer
was made against it.

History — both superseded Suggestions recorded as "asked again".

Two changes were needed to get there, both in the case and neither in the
product. The staleness step now derives its edit from the revision's operations
(recorded in the note above). And the request is now directive and asks for
separable changes: js437t has the Collaborator answer "with commentary, a
Suggestion, or both", so an open question earns prose as readily as a change —
one earlier run answered with advice and called `suggest` at no point — and a
one-Element Suggestion is fully resolved by the cherry-pick, which leaves the
rejection step nothing pending to reply to. The cherry-pick now requires more
than one Element and says so, rather than failing three steps later on a null
card.

Noted while there, pre-existing and not this issue's: in the other [live] case,
the model calls `get_track_analysis` four times and is refused each time with
`unknownTool`. The sidecar advertises the tool and the registry in that case's
fixture does not hold it. Every tool is registered under the ordinary
`setSession` path, so this is that case's harness and not the shipping app.
