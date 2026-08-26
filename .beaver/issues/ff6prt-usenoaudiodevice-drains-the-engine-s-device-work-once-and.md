---
id: ff6prt
title: useNoAudioDevice drains the engine's device work once, and that drain can post more
state: todo
priority: low
labels:
    - maintenance
created: 2026-08-26T18:50:33Z
updated: 2026-08-26T18:50:33Z
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
