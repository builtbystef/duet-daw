---
id: 77euel
title: A turn of the message loop after an undo takes the redo away
state: todo
priority: high
labels:
    - bug
created: 2026-08-27T03:04:20Z
updated: 2026-08-27T03:10:02Z
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
