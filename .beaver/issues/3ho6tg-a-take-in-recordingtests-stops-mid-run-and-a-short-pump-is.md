---
id: 3ho6tg
title: A take in RecordingTests stops mid-run, and a short pump is not the cause
state: todo
priority: low
labels:
    - bug
created: 2026-08-26T10:37:31Z
updated: 2026-08-26T10:37:31Z
---

## What happened

`an undo during a take neither stops it nor moves the playhead`
(tests/RecordingTests.cpp) failed at `REQUIRE (session.isRecording())` after
`runUntilThePlayheadMoves` on 2026-08-20, during issue 5he6vd: the take had
stopped. It failed once inside a single-process `duet_tests` run and once
immediately after, while the machine had just been running the Duet app under
`pw-jack` and a screen capture, then passed three times on its own and twice in
a full `ctest` sweep on the same tree.

Carried out of ax88i4, which recorded it as a note and fixed the three
device-rebuild take cases. It did not turn out to be the same defect. The fix
there was for a wait too short to see something happen; here something that had
already happened was undone, and `runUntilThePlayheadMoves` returns as soon as
the playhead moves, so its length is not the bound. Something stopped the take.

The likely mechanism, unconfirmed: hazard 6 reaching this test from the outside.
`useNoAudioDevice` cancels the engine's own four-second rebuild timer, but a
real device disappearing under a session that is holding one still broadcasts a
device change, and a rebuild frees the playback graph and ends the take rolling
through it. Both reported failures were on a machine whose audio device had just
been taken by the app.

It did not reproduce on 2026-08-26 in 8 runs of the case alone under 64 busy
processes on 12 cores — which is consistent with an outside device change rather
than with load, since that harness loads the CPU and never touches the device.

## Acceptance criteria

- [ ] The cause is established, not guessed: either a reproduction, or a reading
      of the record path that shows what else can stop a take that has begun.
- [ ] The case asserts what it is about — that an undo during a take neither
      stops it nor moves the playhead — without being exposed to whatever
      stopped it.
