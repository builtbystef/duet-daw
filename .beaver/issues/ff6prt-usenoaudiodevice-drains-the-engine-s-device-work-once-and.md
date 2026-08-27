---
id: ff6prt
title: useNoAudioDevice drains the engine's device work once, and that drain can post more
state: done
assignee: claude
priority: low
labels:
    - maintenance
created: 2026-08-26T18:50:33Z
updated: 2026-08-27T03:45:05Z
---

## What was found

Discovered while closing 3ho6tg, and not done there.

`Session::Impl::useHostedAudioDevice` switches the device manager to the hosted
device and then flushes the engine's pending work exactly once —
`DeviceManager::dispatchPendingUpdates()`, followed by
`setMidiDeviceScanIntervalSeconds (0)` to cancel the MIDI apply that flush
re-arms. The engine's async update is not idempotent in one pass:
`DeviceManager::handleAsyncUpdate` ends by calling
`checkDefaultDevicesAreValid`, whose `setDefaultWaveOutDevice` /
`setDefaultWaveInDevice` call `rescanWaveDeviceList()` again, and that can post
another async update. `handleUpdateNowIfNeeded` is called once, so a second
update can still be pending when `useNoAudioDevice` returns.

That update lands on the first pump of the message loop. By then a test has
usually allocated a playback context and may have started a take, and
`handleAsyncUpdate` does `clearAllContextDevices()` / `reloadAllContextDevices()`
— which frees the graph. A take rolling through it is then ended by the
engine's 50 Hz transport timer (see `docs/ENGINE_NOTES.md`, "The transport ends
a take whose playhead is not rolling, on the message loop").

Two related facts about the same seam:

- The session does not see a device change it caused itself until the message
  loop delivers the broadcast. `Session::Impl::lastDeviceChangeMs` is written
  only by the `DeviceWatcher` change callback, so `startRecording` immediately
  after `useNoAudioDevice` reads devices that "have never moved", skips the
  pre-roll entirely, and starts the take inside the churn. Measured on the dev
  machine on 2026-08-26: with no pump the take starts at once; with a 10 ms pump
  first, the same call waits.
- Every headless take test works around this by hand — `DeviceRebuildTests`
  pumps 10 ms after the switch, and `an undo during a take neither stops it nor
  moves the playhead` now calls `Session::rebuildDevices`. One drain inside
  `useNoAudioDevice` would cover all of them.

`useNoAudioDevice` is test-facing (ADR 0006), so no producer path is exposed by
this. What is exposed is every headless take assertion.

## Acceptance criteria

- [ ] `useNoAudioDevice` returns with the engine's device work over: no async
      update and no MIDI apply left pending, and the session's own record of
      when its devices last moved is in step with the switch it just made.
- [ ] `startRecording` called straight after it takes the pre-roll it would take
      on any other moved devices, rather than reading them as never moved.
- [ ] The hand-rolled waits the take tests carry for this — the 10 ms pumps in
      `DeviceRebuildTests`, the `rebuildDevices` call in the undo case — are
      removed where they become redundant, and those cases still pass.

## Notes

**claude** — 2026-08-27T03:45:04Z

Done at the Session device seam.

**The drain.** `Session::Impl::useHostedAudioDevice` now waits the engine's
answer out instead of flushing it once. The wait is the one `rebuildDevices`
already used, lifted into `Impl::waitForTheDevicesToGoQuiet (since)`: the MIDI
list built and the engine quiet about its devices for 50 ms, bounded at two
seconds, measured from the switch rather than counted out in milliseconds. Both
callers share it, so there is one answer to "the device work is over" and one
bound.

**The stamp.** The switch also writes `Impl::lastDeviceChangeMs` itself. The
session hears about a device change only when the message loop delivers the
engine's broadcast, so a switch it made itself read as devices that had never
moved — which is what let `startRecording` skip the pre-roll and start the take
inside the churn. `useHostedAudioDevice` lost its `const` for this.

**Red before green.** Removing the three 10 ms pumps in `DeviceRebuildTests`
failed all three cases at `REQUIRE_FALSE (session.isRecording())` — the take
started at once on devices the session thought had never moved. That is
criterion two, and the stamp is what turns it green. Criterion one has no seam
of its own that fails: a probe that started a take straight after the switch and
pumped 200 ms passed before the change too, because `handleAsyncUpdate` reloads
the context devices as well as clearing them and the transport finds a rolling
playhead again. That probe was written, seen to pass on both sides, and deleted
rather than kept as a test that cannot fail.

**Waits removed.** Three 10 ms pumps in `DeviceRebuildTests`, the `rebuildDevices`
that `a commanded device rebuild ends a take` used to get the churn out of the
way, and the `rebuildDevices` that `an undo during a take neither stops it nor
moves the playhead` carried from 3ho6tg. The four `rebuildDevices` callers left
in the suite are hardware cases that never switch to the hosted device, so none
of them is this workaround.

**Discovered, not done here.** 77euel — a turn of the message loop after an undo
takes the redo away. Deferred engine work performs an unnamed transaction on the
project's undo manager, and 5 ms of pump is enough. This is producer-facing, not
a test artifact. It surfaced because the drain runs the message loop:
`redoing the removal of a sounding MIDI note leaves no voice stuck behind` went
red on `REQUIRE (session.redo())`. That case now takes the device switch before
its undo, where its three siblings in the same file put it; the reordering is
criterion four of 77euel and comes out with the fix.

**Cost.** The drain adds about nine seconds to the suite — roughly 300 ms per
session that switches, across about thirty of them. Measured on the dev machine
on 2026-08-26: 97 s before, 103-106 s after.

**Recorded.** `docs/ENGINE_NOTES.md` — the entry formerly titled "A hosted-device
switch leaves one MIDI apply pending" is now "A hosted-device switch answers
itself, so one flush does not end it", and carries the async-update chain
(`handleAsyncUpdate` to `checkDefaultDevicesAreValid` to `setDefaultWave*Device`
to `rescanWaveDeviceList` to another async update) that made one
`dispatchPendingUpdates` insufficient. No code comment cited the old heading.

**Checks.** clang-format dry-run clean, full lint sweep clean, `duet_tests` and
`duet_app` build, ctest 335/335 pass (9 hardware cases skipped on this machine).
