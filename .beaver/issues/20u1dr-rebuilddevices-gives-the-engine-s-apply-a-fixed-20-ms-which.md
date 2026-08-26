---
id: 20u1dr
title: rebuildDevices gives the engine's apply a fixed 20 ms, which a loaded machine misses
state: done
assignee: claude
priority: low
labels:
    - bug
created: 2026-08-26T10:37:12Z
updated: 2026-08-26T17:48:20Z
---

## What happened

`a test can make the engine's device rebuild happen, or not happen`
(tests/DeviceRebuildTests.cpp) fails at `REQUIRE (midiInputCount (session) > 0)`
on a loaded machine: measured 1 failure in 15 runs of that case alone, with 64
busy processes on 12 cores, on 2026-08-26 during issue ax88i4.

The bound is in production code, not in the test. `Session::rebuildDevices`
asks for the device list and then runs the message loop for exactly
`deviceApplyMs` (20 ms), because the engine applies a rescan on a 5 ms timer and
settles its defaults ~5 ms after that. Its documented promise is that both have
landed before it returns — "a caller that asked for the rebuild to be over would
still be waiting on the engine" — and 20 ms of wall clock is not that promise on
a machine that cannot deliver two timer ticks in 20 ms.

Same class as ax88i4 and found by the same load harness, but a different case
and a different place: ax88i4 was two take assertions in the test file, and the
fix there was `duet::testing::pumpUntil`, which a test can reach and production
code cannot.

## Acceptance criteria

- [ ] `Session::rebuildDevices` returns with the engine's rescan and its
      default-settling apply both landed, on a machine loaded enough to make the
      current 20 ms miss.
- [ ] It still returns as fast as the engine allows on an idle machine, and
      still frees the playback graph, which is what its callers assert on.
- [ ] It does not wait on the engine's own four-second rebuild timer.

## Reproducing

Run the case in a loop with more busy processes than cores:

```
for i in $(seq 1 64); do bash -c 'while :; do :; done' & done
for run in $(seq 1 15); do ./build/tests/Debug/duet_tests 'a test can make the engine*'; done
```

## Notes

**claude** — 2026-08-26T17:48:07Z

Fixed. `Session::rebuildDevices` now waits for the engine's two applies instead
of counting out twenty milliseconds of wall clock.

**Seam.** The Session facade, in tests/DeviceRebuildTests.cpp — the case the
issue names (`a test can make the engine's device rebuild happen, or not
happen`) is the acceptance test, unchanged. Nothing new is observable from
outside: the bug is how the model waits, not what it does, so no test was added.

**What it waits for.** The ask is stamped, and the wait holds until the MIDI
input list is built and the engine has said nothing about its devices for 50 ms
counted from the later of the ask and the last device-change broadcast — so
every apply that lands pushes the answer out again, and a machine that cannot
deliver two 5 ms timer ticks in twenty milliseconds still gets both of them.
Bounded at two seconds, deliberately under the engine's own four-second rebuild
timer (the same reasoning as `duet::testing::pumpUntil`), so a rebuild that
never lands cannot be answered by that timer instead of by what was asked for.

**Decisions.**

- *Quiet, not a count of broadcasts.* Counting device-change broadcasts was the
  first candidate and is wrong twice over: `applyNewMidiDeviceList` returns
  early without broadcasting when neither the list nor the defaults changed —
  which is every ask after the first, and the reason `rebuildDevices` frees the
  playback context itself — and JUCE coalesces broadcasts, so two applies can
  arrive as one callback. A quiet window survives both.
- *Its own window, not the take's.* The wait does not reuse `devicesAreSettled`
  and its 100 ms `deviceQuietMs`: that knob is `setDeviceWait`'s, driven by the
  take tests, and a rebuild that inherited it would answer to a test knob and
  cost 110 ms where the engine needs ~10. 50 ms is ten times the gap the engine
  schedules between the two applies.
- *`nowMs` throughout*, so the wait reads the model's device clock, the same one
  the settle-wait reads. The comparison in `devicesLastMovedSince` is signed on
  the difference of two unsigned counters, so the counter's wrap stays a
  difference like any other.

**Red, then green.** Reproduced with the load harness in the body — 64 busy
processes on 12 cores — `3 of 15` runs of that case failed at
`REQUIRE (midiInputCount (session) > 0)`. With the fix, `15 of 15` pass under
the same load, and the whole `[devices]` tag passes `8 of 8` under it.

**Criteria two and three.** Measured on an idle dev machine with a
`duet_scratch` probe: the first rebuild of a session takes 59 ms and each
further one 50 ms, against 20 ms before — the extra is the settle window, and
the engine's own two applies still cost ~10 ms of it. Nothing waits on the
four-second timer. `freePlaybackContext` is untouched, and `a commanded device
rebuild stops a rolling transport, and the model starts it again` still holds.

**Facts for a reviewer.** The nine device-backed cases that `ctest` skips on
this machine include the two `rebuildDevices` callers outside
DeviceRebuildTests.cpp (`one Action from every domain lands while the transport
rolls`, `the transport keeps rolling while the Collaborator reads the project`);
they skip for want of an audio device, not because of this change, and CI runs
them. Hazard 6 in docs/ENGINE_NOTES.md records what `rebuildDevices` now
promises. Checks: format, lint sweep, full build and `ctest` (332 passed).
