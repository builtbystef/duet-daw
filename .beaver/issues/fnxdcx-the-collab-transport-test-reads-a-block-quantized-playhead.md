---
id: fnxdcx
title: the collab transport test reads a block-quantized playhead and requires it to have moved
state: todo
priority: high
labels:
    - bug
created: 2026-08-26T18:01:38Z
updated: 2026-08-26T18:01:38Z
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
