---
id: ax88i4
title: The device-rebuild take tests are timed against wall-clock and fail under load
state: todo
priority: low
labels:
    - bug
created: 2026-08-20T09:40:13Z
updated: 2026-08-20T10:54:44Z
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
