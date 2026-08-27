---
id: 77euel
title: A turn of the message loop after an undo takes the redo away
state: done
assignee: claude
priority: high
labels:
    - bug
created: 2026-08-27T03:04:20Z
updated: 2026-08-27T05:31:49Z
---

## What was found

Discovered while closing ff6prt, and not done there.

Undo something, then let the message loop turn once — a fifth of a frame is
enough — and the redo is gone. Nothing the producer did took it away: deferred
engine work performs a transaction of its own on the project's `UndoManager`,
and a new transaction is what drops the redo stack.

Measured on the dev machine on 2026-08-26, through `tests/scratch`, at the
`Session` seam:

```
with a MIDI clip, after undo — undo: [Set up the note] [Add a track]  redo: [Remove the note]
  after 30 ms of pump       — undo: [] [Set up the note] [Add a track]  redo:
with no clip at all, after undo — undo: [Add a track]  redo: [Rename the track]
  after 30 ms of pump          — undo: [] [Add a track]  redo:
```

Five milliseconds of message loop is enough, and the transaction is there in
both shapes, with a MIDI clip and without one — so the async clip re-sort is not
it. The unnamed `[]` on the undo stack is the writer: something wrote through
`Session::Impl::undoManager()` outside a `performAction`, so it landed in a
transaction with no name, and `Session::redoNames()` would offer the producer
that empty name if they undid it.

`Edit::dispatchPendingUpdatesSynchronously` does *not* do it — the level cases
call it between an undo and a redo and the redo survives — so the writer is on a
timer or an async updater rather than in the edit's own pending work.

This is a producer-facing bug, not a test one: Undo, glance away, Redo, and the
gesture is gone. It is also what the undo history's own name for the transaction
would show as blank in the Edit menu.

## Where it bites now

`redoing the removal of a sounding MIDI note leaves no voice stuck behind`
(`tests/PlaybackLevelTests.cpp`) is the case that found it. It passed only
because nothing pumped the message loop between its undo and its redo. ff6prt
made `Session::useNoAudioDevice` run the loop, and the case went red; it now
takes the device switch before the undo, the way its three siblings in that file
do. That workaround comes out when this is fixed.

## Acceptance criteria

- [ ] The writer is named: which engine work performs the unnamed transaction,
      on what schedule, recorded as a fact in `docs/ENGINE_NOTES.md`.
- [ ] An undo survives a turn of the message loop: after `Session::undo`, the
      redo is still offered and still named, however much loop runs before the
      producer reaches for it.
- [ ] No transaction the producer did not make ever reaches
      `Session::undoNames` — the empty name is a symptom, and a name is not the
      fix.
- [ ] The reordering in `redoing the removal of a sounding MIDI note leaves no
      voice stuck behind` is put back, and the case still passes with the undo
      before the device switch.

## Notes

**claude** — 2026-08-27T03:10:02Z

A lead, not a conclusion: hazard 1 in docs/ENGINE_NOTES.md already records that an op written while no named transaction is open lands in an unnamed step once the engine's UndoTransactionTimer seals, and Duet's remedy is that every project change goes through Session::performAction (ADR 0004). The unnamed [] transaction here is that shape. What does not fit yet is the timing: hazard 1 puts the timer at 350 ms of quiet message loop, and 5 ms of pump was enough to lose the redo. So either the writer is not that timer, or the timer is not the 350 ms the note says. Start by finding the write, not by assuming which timer sealed it.

**claude** — 2026-08-27T05:31:30Z

Fixed. The writer, the fix, and what a reviewer needs.

**The writer.** Not the 350 ms transaction timer of hazard 1 — the engine's async
updaters, which fire on the next turn of the message loop. Three of them write
through the Edit's UndoManager: TrackList::handleAsyncUpdate (sortTracksByType,
triggered by newObjectAdded and, unlike the clip list's, never suppressed during
undo), ClipList::handleAsyncUpdate (hazard 2), and Edit's TrackStatusUpdater
(updateTrackStatuses — sanityCheckTrackNames and updateMuteSoloStatuses). JUCE's
UndoManager::undo ends with beginNewTransaction, so the first of those writes to
land after an undo opens a step of its own and stashes the redo. Recorded as
hazard 9 in docs/ENGINE_NOTES.md.

**The fix.** Session::Impl::settleEngineBookkeeping does that work itself, while
the Action's transaction is still open: performAction and stopRecording end with
it, and startUndoHistory runs it once so the state the first Action builds on is
settled too. The engine's own pass then finds nothing to write whenever the loop
delivers it, so no step outside an Action is ever created. That keeps ADR 0004
intact — Actions remain the only transaction boundary — and needs no message loop
inside undo.

**The alternative that does not work, and why.** Letting the foreign step land
and then dropping it with UndoManager::undoCurrentTransactionOnly (which restores
JUCE's stashed redo, the shape Suggestion.cpp already uses) livelocks: the
engine's clip sort re-triggers itself on every move it makes, so undoing it makes
it run again, forever. Measured at ~330k rounds in one test before the timeout.
Do not revisit it.

**What Duet had to copy.** The clip owner's sort rule is private to the engine's
ClipOwner.cpp, so Session.cpp writes it out (child-kind priority, then start
order). If the engine's rule ever drifts, `the redo outlives a re-sort an Action
left pending behind it` goes red — the drift is caught, not silent. The track
sort uses the engine's own public TrackList::sortTracksByType.

**One test-apparatus bug fixed on the way.** PluginList::insertPlugin always
writes through the Edit's UndoManager; its third argument reads like an undo
manager and is a SelectionManager*. The realtime probe passed null there and
still landed on the undo stack, outside any Action. It now writes its state onto
the track directly with a null UndoManager, which is what its comment always
claimed. Recorded as a further fact.

**Seams and cases.** Session, in tests/EditVocabularyTests.cpp: `a turn of the
message loop after an undo leaves the redo where it was` (the no-clip shape) and
`the redo outlives a re-sort an Action left pending behind it` (the clip shape).
Both pump 400 ms — past the 350 ms timer as well — and assert undoNames and
redoNames exactly, so a step the producer did not make fails them.

The reordering in `redoing the removal of a sounding MIDI note leaves no voice
stuck behind` is reverted to its pre-ff6prt shape: the undo is back ahead of the
device switch, and the case passes.

Checks: format clean, lint clean, 337/337 ctest.
