---
id: xbh9qk
title: An offline render stops the transport, so a measured analysis interrupts playback
state: todo
priority: high
labels:
    - bug
created: 2026-08-28T10:24:08Z
updated: 2026-08-28T10:24:08Z
---

## What is wrong

A `get_track_analysis` call renders the track offline, and an offline render
stops the transport for as long as it runs (engine notes, "An offline render
stops the transport for as long as it runs"). So a producer who asks the
Collaborator about a track while the project is playing hears playback stop for
the length of the render — seconds, on a real track — and then start again.

Spec js437t says the opposite: "A multi-second first call is acceptable; playing,
editing, and recording continue throughout." Editing does continue, and is
asserted in `tests/TrackAnalysisTests.cpp`; playing does not.

Worse for a long render than a short one. `Session::startPlayback` keeps asking a
stopped transport to roll for ten seconds — hazard 6's remedy — so a render
under ten seconds reads as a gap and a render over ten seconds leaves the
transport stopped for good, with nothing asking it to roll again.

Recording is untested and the same mechanism applies to it: a take rolling when
an analysis starts would be cut.

## What is known

- It is the engine's own render status and not Duet's `stopAllTransports` call
  beside it: measured on the dev machine 2026-08-28 by a `duet_scratch` probe
  that started playback, rendered one track on a worker thread and sampled
  `isPlaying()` from the message thread every five milliseconds throughout —
  131 samples, none rolling, with and without that line.
- Every offline render has this property, so it reaches the export and bounce
  path (zm174o) as well as the Collaborator's analysis.

## What a fix has to decide

Three shapes are visible, and which is right is a decision this issue is asking
for rather than one it makes:

1. **Render a copy.** Analysis renders a detached copy of the Edit, the way
   Suggestion Audition already materialises one, so the project the producer is
   playing is never put into render status. The most faithful to the spec and
   the most work.
2. **Defer the render.** A measurement asked for while the transport is rolling
   waits for it to stop. Faithful to "playback never stops" and unfaithful to "a
   Task Run is non-blocking", since the model would wait indefinitely.
3. **Accept the gap and put playback back.** The tool remembers where the
   transport was and restarts it after the render. Cheap, keeps a long render
   from leaving the transport stopped, and still an audible gap — which would
   make it a change to what the spec promises.

## Acceptance criteria

- [ ] A `get_track_analysis` call made while the transport is rolling leaves
      playback where the spec says it should be, and a test asserts that from
      the message thread *during* the call rather than after it.
- [ ] A render longer than the play retry's ten seconds does not leave the
      transport stopped.
- [ ] A take rolling when a measurement starts is not cut by it.
- [ ] Whatever is decided is written into spec js437t's notes, since the spec's
      own sentence is what this contradicts.
