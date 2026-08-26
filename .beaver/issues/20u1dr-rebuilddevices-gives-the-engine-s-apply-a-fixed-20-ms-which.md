---
id: 20u1dr
title: rebuildDevices gives the engine's apply a fixed 20 ms, which a loaded machine misses
state: todo
priority: low
labels:
    - bug
created: 2026-08-26T10:37:12Z
updated: 2026-08-26T10:37:12Z
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
