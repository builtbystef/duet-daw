---
id: xbh9qk
title: An offline render stops the transport, so a measured analysis interrupts playback
state: done
assignee: claude
priority: high
labels:
    - bug
created: 2026-08-28T10:24:08Z
updated: 2026-08-28T12:01:53Z
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

## Notes

**claude** — 2026-08-28T11:11:36Z

## Decision (2026-08-28, the user's)

**Option 1 — render a detached Edit.** Analysis renders a copy of the Edit
rather than the project the producer is playing, so the project is never put
into the engine's render status and the transport is never stopped. It is the
only one of the three shapes that keeps both of the spec's promises at once:
playback continues *and* a Task Run stays non-blocking.

Why not the other two. Option 3 (accept the gap and restart playback) trades
away the thing the Collaborator exists for — a multi-second dropout in the
middle of a session is exactly the flow cost that makes existing tools useless.
Option 2 (defer the render until the transport stops) is worse: a producer plays
constantly, so the run would hang indefinitely, contradicting "non-blocking"
instead of "playback continues".

`duet_model` already materialises a reusable detached Edit for Suggestion
Audition, so this is an existing path in the codebase rather than a new idea.

## Do this first

The load-bearing assumption is untested: that the engine will render a detached
copy while the original plays, and at an acceptable cost when the track hosts
VST3s that would have to be instantiated twice. That is a `duet_scratch` probe,
not a design session — play the original, render the copy on a worker thread,
sample `isPlaying()` from the message thread throughout, and measure what the
copy costs to build for a track with a hosted plugin.

If the probe says no, fall back to option 3 with that evidence, and say so in a
note here before building it.

## Independent of which shape wins

A render longer than the ten-second play retry currently leaves the transport
stopped with nothing asking it to roll again. That is a plain bug and it is the
second acceptance criterion above; it wants fixing whichever way the first one
goes.

**claude** — 2026-08-28T12:01:53Z

## Done — option 1, the detached copy

### The probe said yes

Run first, as this issue's decision note asked. `duet_scratch`, dev machine
2026-08-28: playback started, a copy of the project rendered on a worker thread,
`isPlaying()` sampled from the message thread every five milliseconds
throughout.

- 133 of 133 samples rolling with no plugin, 142 of 142 with a hosted VST3 in
  the chain — against the 3bgymu measurement of 0 of 131 when the same render is
  made off the project itself.
- The copy costs 15 ms: copy plus render took 736 ms with the plugin
  instantiated a second time, the same render off the project 721 ms.

One thing the probe had to teach before it worked, now in the engine notes: an
Edit loaded with `loadEditFromState` is given no `editFileRetriever`, so its
clips resolve their sources against nothing and the render fails with "Didn't
find any audio to render". The retriever has to be in the `Edit::Options` the
copy is built from — assigning it afterwards is too late.

### What was built

`Session::renderDetachedToFile` and `Session::renderDetachedTrackToFile` — the
existing pair, made off a `DetachedProject`: a copy of the project's own state
opened `Edit::EditRole::forRendering`, sharing this session's Engine the way
Audition's detached Edit does, built and destroyed on the message thread. It has
no playback context to free and no transport to stop, so the project is never in
render status. `RenderGuards` now takes `stopEveryTransport`, because
`TransportControl::stopAllTransports` is engine-wide and a detached render must
not make that call. `offlineTrackRenderer` in `duet_collab` calls the new pair,
which is the whole of what changes for `get_track_analysis`.

Independently, `Session::Impl::keepPlaybackRolling` no longer counts an ask
while the Edit is rendering: nothing is spent during a render, and the first
tick after it ends asks with the whole window in hand. That is the plain bug at
criterion 2, and it fixes the export and bounce path (zm174o) as well, which
still renders the project itself and still stops the transport — the correct
trade for a render the producer is waiting on.

`Session::setPlayRetry (intervalMs, attempts)` is the test seam that makes that
window assertable in milliseconds instead of ten real seconds, the same shape as
`setDeviceWait`.

### Criteria

- [x] A call made while the transport is rolling leaves playback rolling, and
      the test asserts it from the message thread *during* the call:
      "a measurement never stops the transport" samples `isPlaying()` through
      the harness's `meanwhile` hook and requires every sample rolling. Red
      before the change at 111 of 242.
- [x] A render longer than the retry window does not leave the transport
      stopped: "a render longer than the play retry does not leave the transport
      stopped", with the window shrunk to three asks. Red before the change.
- [x] A take rolling when a measurement starts is not cut by it: "a take rolling
      when a measurement starts is not cut by it" samples `isRecording()` the
      same way. Red before the change at 1 of 240.
- [x] Written into spec js437t's notes.

### Seams

The first and third are the spec's primary seam — real service, real socket, the
test-double sidecar as a child process, a real project — where the rest of
`TrackAnalysisTests` already lives. The second is not this spec's at all: it is
the model's own transport behaviour, so it sits in `ProjectAndTransportOpsTests`
beside "one call to start playback is enough".

### For a reviewer

- The old case "a measurement leaves the transport where it found it" is gone.
  It pinned the weaker promise deliberately — playback comes back — and its own
  comment named this issue as the reason. What replaced it pins the promise the
  spec makes.
- The take test asserts that recording never stops and that one clip lands. It
  does not assert notes played on both sides of the measurement: pushing blocks
  in small pieces around a pumped message loop loses the later notes for reasons
  that have nothing to do with this change, and an assertion that fails for an
  unrelated reason is worse than one that is narrower.
- `trackStateDigest` still strips `soloIsolate` and `playSlotClips`. A
  measurement no longer writes them onto the project — it writes them onto the
  copy — but the export path still does, and the engine note says so now.
