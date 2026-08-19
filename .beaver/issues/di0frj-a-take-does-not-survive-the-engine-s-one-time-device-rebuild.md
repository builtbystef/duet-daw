---
id: di0frj
title: A take does not survive the engine's one-time device rebuild
state: done
assignee: claude
priority: medium
labels:
    - bug
parent: b1j3me
created: 2026-08-19T07:00:43Z
updated: 2026-08-19T09:10:18Z
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

## Notes

**claude** — 2026-08-19T09:10:18Z

Done. One press of Record is enough, on the first gesture of a session as much as on any other, because the engine's device rebuild now happens before the take rather than during it.

**The decision (the one the issue names): a pre-roll, not a continued take.** The alternative was never really open. `TransportControl::record` starts a take at the playhead, so a second ask lands the first half as one clip and begins another — the two-clips-and-no-warning outcome this issue calls worse than losing the take. So `Session::startRecording` gets the rebuild out of the way first, and the take begins on the far side of it.

**What the rebuild actually is, measured rather than assumed.** It is `DeviceManager::applyNewMidiDeviceList`, which the engine schedules on a timer `DeviceManager::initialise` sets to four seconds — so it lands four seconds into a session, not into its first playback. It clears and reloads every playback context's devices, which frees the graph the take is recording through. Observed on the dev machine: a take started as the first gesture stopped at t=4.0 s, and at that same instant the session's input count went from 33 to 35 — the two MIDI inputs appearing is the rebuild.

**And it is not one event.** The rebuild settles the engine's default devices afterwards (`checkDefaultDevicesAreValid` -> `setDefaultMidiOutDevice` -> `rescanMidiDeviceList`), which schedules a second rebuild five milliseconds behind the first. A take started between the two is ended by the second, and reads as a rolling transport that produces no clip at all. So the take waits for the engine to be *still*, not merely to have started.

**How it works.** `startRecording` asks whether the devices are settled — the engine's MIDI input list built (before the build it has none; after it, always at least its own "All MIDI Ins", which is the only sign of the build the engine offers), and unchanged for 100 ms since the last time the engine broadcast a device change. If they are, the take starts here and now, synchronously, as before. If they are not, the session asks the engine for the build immediately rather than four seconds from now, and a 20 ms timer starts the take once the churn stops. Bounded at two seconds, after which the take starts regardless: a pre-roll that never ended would be worse than the interrupted take it prevents.

**Consequences a caller should know.** A take may begin very slightly after `startRecording` returns, and `isRecording` says which — it is the transport recording, not the asking for it. In an open session the wait is zero: the devices settled seconds ago. Stop reaches a take that has not begun (both ways in pass through `stopPlayback`, so one cancel covers both), and so does Play. The `Session.h` doc says all of this.

**Where it does not live.** Not in the `Session` constructor. Asking for the build there was the first thing tried, and it destabilised the headless record path: it moves the engine's device churn into the window where a no-audio-device test is pushing blocks, and *an undo during a take neither stops it nor moves the playhead* began failing in full-suite runs. Keeping both the ask and the wait inside `startRecording` means nothing outside the record path changes behaviour.

**Tests** (`tests/RecordingTests.cpp`):

- *a take started as the first transport gesture of a session survives the rebuild* — criterion 2. A real device, skipped where there is none, as the playback tests are. It arms an input, presses Record as the session's first transport gesture, pumps eight seconds through the rebuild window, and requires one clip that starts where Record was pressed and runs the whole window (an interrupted take is about four seconds, or no clip). Red before the change, green after; verified discriminating against both halves of the fix independently — remove the ask and it fails, remove the wait and it fails.
- *a take waiting for the engine's devices is stopped by a Stop* — the guard on the cancel: everything the waiting take needs is in place, a Stop arrives, and the take must not start anyway. Verified discriminating by removing the cancel from `stopPlayback`.

**What is not covered, and why.** The pre-roll's *lower* bound — a Record pressed in the first milliseconds of a session — has no honest test. A session that has not turned its message loop has not been offered its inputs either, so a test cannot arm a track to record from; and every contrivance that gets around that (choosing the input after pressing Record) puts the engine in a state no producer gesture reaches, where the rebuild wipes the arming's live effect. Such a test was written, watched fail for that unrelated reason, and deleted. The behaviour is covered where it is reachable: the real-device test above presses Record with the device list unbuilt, which is the pre-roll's wait branch.

`docs/ARCHITECTURE.md` records the record path's half of hazard 6 beside playback's. No ADR: the decision is one function wide.

Checks: format, lint, and 65/65 ctest green, the full suite run four times.

To confirm by ear: open a project and press Record within a second or two of it opening, play something, and Stop — one clip, holding everything played, where before the take ended a few seconds in.
