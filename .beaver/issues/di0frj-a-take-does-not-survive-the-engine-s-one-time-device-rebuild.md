---
id: di0frj
title: A take does not survive the engine's one-time device rebuild
state: todo
priority: medium
labels:
    - bug
parent: b1j3me
created: 2026-08-19T07:00:43Z
updated: 2026-08-19T07:00:43Z
---

## What happens

The engine rebuilds its device list once, a few seconds into the first playback
of a session, and the rebuild frees the playback graph and stops the transport
with it (spec b1j3me, hazard 6). `Session::startPlayback` survives this by
asking again until the transport rolls (issue sohgf4).

`Session::startRecording` (issue nfjr5x) deliberately does not. Asking again
would not continue the take: `TransportControl::record` starts a take at the
playhead, so a second ask lands a partial take as one clip and starts another —
worse than losing the take, because the producer gets two clips and no warning.
So the take simply ends where the rebuild lands, and a producer whose first
gesture of a session is Record loses it.

Found while building nfjr5x. The headless record path does not see it, because a
session running with no audio device has no device list to rebuild, so no
automated test catches it today.

## What it should do

One press of Record is enough: a take that the rebuild interrupts either
continues, or the rebuild happens before the take can start. Which of the two is
a decision this issue has to make — a pre-roll that gets the rebuild out of the
way before the first take of a session is the cheaper of them, and the transport
bar (1fumn6) already has a countdown to hide it behind.

## Acceptance criteria

- [ ] A take started as the first transport gesture of a session survives the
      engine's device rebuild: it records what the producer played, from the
      moment they pressed Record, as one clip.
- [ ] A regression test that reproduces the rebuild — which means a session on a
      real device, so it skips itself where there is none, as the playback tests
      already do.
