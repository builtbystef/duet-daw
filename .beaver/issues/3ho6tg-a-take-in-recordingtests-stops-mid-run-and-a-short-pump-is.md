---
id: 3ho6tg
title: A take in RecordingTests stops mid-run, and a short pump is not the cause
state: done
assignee: claude
priority: low
labels:
    - bug
created: 2026-08-26T10:37:31Z
updated: 2026-08-26T18:52:00Z
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

## Notes

**claude** — 2026-08-26T18:51:51Z

Fixed. The cause is established by reproduction, and the case no longer starts a
take inside the window the failure comes from.

**The cause.** What ends a take that has begun is the engine's transport timer:
`TransportControl` runs at 50 Hz on the message thread, and every tick a
transport that says it is recording while the playback graph's playhead is not
playing is stopped outright. Freeing or reloading the playback context does not
end a take by itself; the next turn of the message loop does. So every pump a
take test runs is exposure, and anything that leaves the graph without a rolling
playhead — a device apply, above all — cashes it in. Recorded as an engine note;
the entry is "The transport ends a take whose playhead is not rolling, on the
message loop", and it names the lines.

**The reproduction.** The case was made to fail on demand, at exactly the
assertion it failed at on 2026-08-20 — `REQUIRE (session.isRecording())` after
`runUntilThePlayheadMoves` — by calling `Session::rebuildDevices` on the rolling
take. With the rebuild placed before the playhead read instead, it fails one line
earlier, at `reached > 0.0`, because the take never gets to move the playhead at
all. Both are the same defect seen at two moments.

**Why this case and not its neighbours.** Measured on the dev machine on
2026-08-26: with no pump between `useNoAudioDevice` and `startRecording`, the
take starts at once; with a 10 ms pump first, the same call waits. The session
only learns its devices moved when the message loop delivers the engine's
broadcast, so a Record issued before that reads devices that have never moved,
skips the pre-roll that hazard 6's recording remedy is, and starts the take
inside the churn. The `[devices]` cases pump 10 ms for this reason already; this
one did not.

**The fix.** The case calls `Session::rebuildDevices` straight after
`useNoAudioDevice`: that asks for the list, runs the message loop until the
engine has built it and gone quiet about it, and frees the playback graph — while
there is no take to end. It then waits for the take with `pumpUntil` instead of
asserting it started at once, since which of the two happens is not what the case
is about. `runUntilThePlayheadMoves` waits with `pumpUntil` too, so an attempt
costs the ~20 ms a position read needs rather than a flat 200 ms; the loop is
what buys margin, since blocks and not milliseconds are what move the playhead.

**What it still asserts.** Unchanged: an undo during a take neither stops it nor
moves the playhead, and the rename it undoes did land. Two runs of six, one of
them under 64 busy processes on 12 cores, plus a full `ctest` sweep — 333 passed.

**Residual exposure, stated.** It cannot be zero: with no audio device the
transport's position is only fetched from the message loop, so a take test has to
run some. What is gone is the churn the hosted switch leaves behind, which was
the loaded end of it.

**Kept as a test.** `a commanded device rebuild ends a take, and the model starts
no other`, in `DeviceRebuildTests` beside its playback twin. Play survives a
rebuild because the model asks again; Record does not, deliberately, because a
second ask would land the first half as one clip and begin another. That contrast
is what made this case fragile and nothing pinned it.

**Discovered, not done here.** ff6prt — `useNoAudioDevice` flushes the device
manager's async update once, and that flush's own `checkDefaultDevicesAreValid`
can post another, so the switch can leave work behind for the first pump; and the
session cannot see a device change it caused itself until the loop delivers it.
Draining inside `useNoAudioDevice` would retire the hand-rolled waits in these
tests, including the `rebuildDevices` call added here. Not blocking: this case is
correct either way.
