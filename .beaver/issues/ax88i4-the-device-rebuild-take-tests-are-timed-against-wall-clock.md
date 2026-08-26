---
id: ax88i4
title: The device-rebuild take tests are timed against wall-clock and fail under load
state: done
assignee: claude
priority: low
labels:
    - bug
created: 2026-08-20T09:40:13Z
updated: 2026-08-26T10:50:50Z
---

## What happened

`a take starts at the bound even if the devices never settle` (tests/DeviceRebuildTests.cpp) failed twice in a full `ctest --preset linux-debug` sweep on 2026-08-20, on the same tree that then passed the sweep twice in a row, and passes on its own every time. The assertion that fails is `REQUIRE (session.isRecording())` after `duet::testing::pumpMessages (5)`.

Both failures happened while the machine was also running the Duet app under `pw-jack` and a screen capture, so the audio device was contended. The test is written against wall-clock time — a 5 ms pump has to be long enough for the deferred take to start at a bound of 0 ms — so a busy machine is enough to miss it. The two neighbouring cases in the same file use the same 5 ms pump and are exposed the same way.

This is not a wrong answer from the model; it is a bound that has no margin. It is worth closing before CI meets a slow runner.

## Acceptance criteria

- [ ] The three device-rebuild take cases pass on a machine loaded enough to make the current 5 ms pump miss, without waiting on the engine's own four-second timer.
- [ ] What the test asserts is unchanged: that the take starts at the lower bound, and not before it.

## Notes

**claude** — 2026-08-20T10:54:44Z

A second take test of the same shape failed the same way, on 2026-08-20 during issue 5he6vd: `an undo during a take neither stops it nor moves the playhead` (tests/RecordingTests.cpp), at `REQUIRE (session.isRecording())` after `runUntilThePlayheadMoves` — the take had stopped. It failed once inside a single-process `duet_tests` run and once immediately after, while the machine had just been running the Duet app under `pw-jack` and a screen capture, and then passed three times on its own and twice in a full `ctest` sweep on the same tree.

Same root cause class as the body's: a take assertion timed against wall-clock while the audio device is contended. Worth covering when the three device-rebuild cases are fixed, since it is the same bound with the same lack of margin.

**claude** — 2026-08-26T10:50:40Z

Fixed. The two take assertions that were timed against wall clock now wait for
the take instead of for a number of milliseconds.

**Seam.** The tests stay where they were, at the Session facade — this is a bug
in how they wait, not in what they observe, and nothing about the record path
changed. What is new is `duet::testing::pumpUntil (condition, timeoutMs)` in the
shared test support: it runs the message loop in 5 ms looks until the condition
holds, returns as soon as it does, and gives up after two seconds. Two seconds is
deliberately under the engine's own four-second device-rebuild timer, so nothing
waiting on it can be answered by that timer instead of by what it was waiting for.

**Red, then green.** Reproduced by loading the machine — 64 busy processes on 12
cores — and running `[devices]`: 6 of 8 runs failed, every failure at
`REQUIRE (session.isRecording())` in `a take starts once the engine's devices go
still` or `a take starts at the bound even if the devices never settle`. With the
fix, the three take cases pass 15 of 15 under the same load.

**Criterion two holds.** What each case asserts is unchanged. "Not before the
bound" is still the `REQUIRE_FALSE (session.isRecording())` made immediately
after `startRecording`, before any pumping; `pumpUntil` only replaces the
positive half, and it says nothing about how soon the condition held. The
`waits while the engine's devices are still churning` case needed no change: its
assertions are negative, so a loaded machine can only make it more true, and it
passed throughout.

**Facts for a reviewer.** The 10 ms pump the three cases use to let the hosted
device switch broadcast its change — the settle-wait's starting point — never
failed under that load, so it was left alone. `useNoAudioDevice` already cancels
the engine's four-second scan timer, so these cases were never exposed to it.

**Discovered, not done here.** Two issues published, neither blocking this one:

- 20u1dr — `Session::rebuildDevices` runs the message loop for a fixed 20 ms to
  let the engine's rescan and its default-settling apply land. Under the same
  load `a test can make the engine's device rebuild happen, or not happen` fails
  1 in 15 runs on its own. Same class, but the bound is in production code, where
  `pumpUntil` cannot reach.
- 3ho6tg — carries forward this issue's note about `an undo during a take neither
  stops it nor moves the playhead` in tests/RecordingTests.cpp. It is not the
  same defect: `runUntilThePlayheadMoves` returns as soon as the playhead moves,
  so its length is not the bound — something stopped a take that had already
  begun. It did not reproduce in 8 runs under the same load, which fits an
  outside device change rather than CPU contention.
