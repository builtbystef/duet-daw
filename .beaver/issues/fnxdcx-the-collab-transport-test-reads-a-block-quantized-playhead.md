---
id: fnxdcx
title: the collab transport test reads a block-quantized playhead and requires it to have moved
state: done
assignee: claude
priority: high
labels:
    - bug
created: 2026-08-26T18:01:38Z
updated: 2026-08-26T19:51:47Z
---

## What happens

`the transport keeps rolling while the Collaborator reads the project`
(tests/ProjectToolsTests.cpp:557) fails at its last assertion on a machine that
has an audio device:

```
tests/ProjectToolsTests.cpp:599: FAILED:
  REQUIRE( session.playbackPositionSeconds() > before )
with expansion:
  0.03482993197278911 > 0.03482993197278911
```

5 of 5 runs of the case alone on the dev machine, 2026-08-26, found while
completing 9alksy. It is not a skip: `session.audioDeviceDescription()` is
non-empty here and playback really advances — the sibling case `one Action from
every domain lands while the transport rolls` reaches 2.13 s of playhead in the
same binary. Both numbers above are 1536 samples at 44.1 kHz exactly.

## Why

The playhead the model reports moves once per audio block, and the test samples
it immediately before and immediately after a five-call `ToolRun`. When the run
costs less than one block, both reads land in the same block and the position is
the same number twice. Nothing is wrong with the transport — `isPlaying()` on the
line above passes — and nothing is wrong with the tools; the assertion just
requires wall-clock progress it never waits for.

The same class as ax88i4 and 20u1dr: a test that counts on a duration instead of
waiting for the thing it asks about. `duet::testing::pumpUntil` is the tool the
other two ended up using.

## Acceptance criteria

- [ ] The case asserts that the transport keeps rolling *and* that the playhead
      moves on after the tool run, without depending on the run outlasting one
      audio block.
- [ ] It still fails if a tool call stops the transport or blocks the playhead.
- [ ] It passes on a machine with an audio device, repeatedly, and still skips on
      one without.

## Reproducing

```
./build/tests/Debug/duet_tests 'the transport keeps rolling while the Collaborator reads the project'
```

## Notes

**claude** — 2026-08-26T19:51:47Z

Fixed in tests/ProjectToolsTests.cpp. The last assertion is now
`pumpUntil ([&] { return session.playbackPositionSeconds() > before; })`, with
`REQUIRE (session.isPlaying())` still made first and made before anything pumps
the message loop — so a stop that a tool call caused cannot be asked away by
`Session::startPlayback`'s own retry timer before the assertion sees it. Nothing
outside the test changed.

Proved, on the dev machine on 2026-08-26:

- Red first: the case alone failed 2 of 3 runs before the change
  (`0.02321995464852608 > 0.02321995464852608`, and once `0.0 > 0.0` — the
  playhead had not been published at all yet, which the old assertion could not
  wait out either).
- Green after: 8 of 8 runs of the case alone, plus two full
  `ctest --preset linux-debug` sweeps (333/333) in which the case passed in
  ~1.2 s.
- Criterion 2, by mutation. `session.stopPlayback()` inserted after
  `run.finished()` fails at `REQUIRE (session.isPlaying())`. A playhead that
  never passes the mark (the predicate mutated to `before + 3600.0`) fails at
  the `pumpUntil` assertion after its 2 s bound, so the wait is not vacuous.
- Criterion 3, skip branch. Running the suite with `XDG_RUNTIME_DIR` unset gives
  the process no audio device, and the case skipped there rather than failing,
  alongside the eight sibling device tests.

Facts for a reviewer:

- No new engine note. The fact is already recorded: "The transport's position
  stands still without a real device" (engine notes, further facts) says the
  position is published from the message thread and prescribes exactly this —
  pump until the position moves rather than pump once for long enough.
- `Session::useNoAudioDevice()` is not a stand-in for a machine without one: it
  installs the hosted device interface, and `audioDeviceDescription()` is
  non-empty for it, so the skip does not fire and the position never advances.
  A test wanting the skip branch needs a process with no device, not that call.
- Sandbox-only build note: `cmake --build` fails in Catch2's post-link test
  discovery, which writes its listing into `XDG_RUNTIME_DIR` when that is set;
  the executable links fine. `XDG_RUNTIME_DIR= cmake --build ...` builds
  cleanly, but do not unset it for `ctest` — that is what takes the audio device
  away and turns the device tests into skips.
