---
id: 2jqmj2
title: Make the engine's device rebuild commandable at the test seam
state: done
assignee: agent
priority: high
labels:
    - maintenance
parent: b1j3me
created: 2026-08-19T10:56:10Z
updated: 2026-08-19T12:13:35Z
---

## Why

The engine rebuilds its device list on a four-second timer set by
`DeviceManager::initialise`, and the rebuild frees the playback graph and stops
every transport rolling through it (hazard 6). Duet now absorbs this in two
places — `Session::startPlayback` keeps asking (`sohgf4`), and
`Session::startRecording` waits for the devices to go still before a take
begins (`di0frj`) — but nothing can make the rebuild *happen*. Tests can only
pump the message loop until it has gone by.

Measured on the dev machine, 2026-08-19, `ctest --preset linux-debug`, 65/65
green in 90.59 s. Seven cases spend their time waiting out that window:

```
one Action from every domain lands while the transport rolls    10.33 s
one call to start playback is enough … survives device rebuild   8.46 s
a stopped transport is not asked to play again                   8.46 s
a take started as the first transport gesture … the rebuild      8.28 s
a headless session plays through the one-time device rebuild     4.47 s
an undo during a take neither stops it nor moves the playhead    3.14 s
a take waiting for the engine's devices is stopped by a Stop     2.93 s
                                                        total   46.07 s
```

Fifty-one per cent of the suite, spent on a timer nobody can advance.

Time is the smaller cost. The larger one is coverage: `ci.yml` says the runner
has no audio hardware, so the seven device-gated cases skip there — and they
overlap heavily with the list above. **The device-rebuild behaviour, which has
already produced two bugs (`sohgf4`, `di0frj`), has no CI coverage at all.**

And the settle-wait itself is untestable. `Recording.cpp` measures quiet with
`juce::Time::getMillisecondCounter()` against `deviceQuietMs`, polls on a
`devicePollMs` timer, and gives up after `deviceWaitAttempts`. `di0frj`'s
closing note records that the pre-roll's lower bound — a Record pressed in the
first milliseconds of a session — "has no honest test", because a session that
has not turned its message loop cannot have a track armed on it, and every
contrivance around that puts the engine in a state no producer gesture reaches.
That bound is real behaviour, shipped, and unverified.

## What to build

Two things, both behind the existing device seam in `duet_model`.

**The rebuild becomes commandable.** `Session::Impl` already has the private
pieces — `deviceListIsBuilt()`, `askForTheDeviceList()`, `devicesAreSettled()`,
and the `DeviceWatcher` that stamps `lastDeviceChangeMs`. Extend the seam so a
test can either suppress the engine's own timed rescan or drive it on command,
and so a test that only needs the rebuild to be *over* can say so in one line
instead of pumping four seconds.

One constraint from `di0frj`, and it is load-bearing: asking for the device list
in the `Session` constructor was tried and destabilised the headless record
path — it moves the churn into the window where a no-audio-device test is
pushing blocks, and *an undo during a take neither stops it nor moves the
playhead* began failing in full-suite runs. Whatever this issue adds must not
put the ask on the constructor path by default.

**The settle-wait stops reading the wall clock directly.** `deviceQuietMs`,
`devicePollMs` and `deviceWaitAttempts` become drivable, so the pre-roll's
behaviour — waits when the devices are churning, starts when they go still,
starts anyway at the bound — is asserted without spending real seconds and
without a real device.

The seven device-gated cases stay device-gated. They are ADR 0006's last-mile
check: their reason to exist is being the thing `playWithoutAudioDevice` can
lie about, and un-gating them would remove it. This issue makes the rebuild
deterministic; it does not move the hardware assertions off hardware.

## Acceptance criteria

- [ ] A test can make the engine's device rebuild happen, or not happen,
      without waiting on the engine's own four-second timer.
- [ ] The rebuild's effect on a rolling transport is asserted in a case that
      runs with no audio device, and therefore runs in CI.
- [ ] The pre-roll's three behaviours are each a test that spends no real
      seconds: a take waits while the devices churn, a take starts once they go
      still, and a take starts regardless at the bound.
- [ ] The pre-roll's lower bound — Record as the first gesture of a session,
      before the device list is built — is covered, or the issue records what
      still makes it unreachable and what covers it instead.
- [ ] No new engine or JUCE type appears in a public facade header; the tests
      target `duet_tests`, which links no engine target.
- [ ] `Session::startRecording` behaves identically for a producer: an open
      session still starts a take synchronously, and a Stop or a Play still
      cancels a waiting one.
- [ ] The full suite is green and measurably faster than the 90.59 s recorded
      above; the closing note states the new number and how much of the 46.07 s
      was reclaimed.
- [ ] The seven cases that require real hardware still require it, and still
      skip where there is none.

## Notes

**agent** — 2026-08-19T12:13:35Z

Done. The engine's device rebuild is commandable at the Session device seam, the pre-roll's settle-wait is drivable, and the suite no longer spends half its time on a timer nobody can advance.

**Seam.** The vocabulary layer's public interface (`Session`), which is the spec's Testing Decisions seam and the existing device seam (`useNoAudioDevice` / `playWithoutAudioDevice`). No new engine or JUCE type in a public facade header. The new cases live in `tests/DeviceRebuildTests.cpp` and target `duet_tests`.

**What landed.**

- `Session::suppressDeviceRebuild` — stops the engine's four-second timer. `setMidiDeviceScanIntervalSeconds(0)` persists to Settings.xml, so the production default (4) is written back immediately; this session's member stays 0 and the timer stays stopped. Does not ask for the device list. Not on the constructor path (`di0frj`'s constraint).
- `Session::rebuildDevices` — `rescanMidiDeviceList` plus a 20 ms pump so both applies land, then `freePlaybackContext`. A second ask is a no-op once the list is built, so freeing the context is what still makes the rebuild's effect on a rolling transport visible with no audio device. The first ask of a session is the real first build.
- `Session::setDeviceWait(quietMs, pollMs, attempts)` — the three pre-roll knobs. Production defaults unchanged (100 / 20 / 100). The settle-wait reads `Impl::nowMs()` (a `deviceNow` function) rather than `juce::Time::getMillisecondCounter()` directly.

**New cases** (no audio device, run in CI):

- a test can make the rebuild happen, or not happen — MIDI inputs stay empty under suppress, appear after `rebuildDevices`.
- a commanded rebuild stops a rolling transport, and the model starts it again — `isPlaying()` is false after `rebuildDevices`, true after the keeper's 200 ms. This is sohgf4 on CI.
- a take waits while the devices churn / starts once they go still / starts at the bound — each spends tens of milliseconds.
- Record as the first gesture waits when the device list is not built — the lower bound's door. The take itself cannot start: nothing is armed, and arming needs an input, and an input needs the device list (or hosted, which builds it). The take starting at the bound is the hosted case above; the take surviving on a real unbuilt list is the hardware first-gesture case. Same unreachability `di0frj` recorded; now named as its own case.

**Existing cases sped up, still hardware-gated where they were.** The seven SKIP-on-no-device cases still skip. They drive the rebuild or hold it instead of pumping 4–8 s. The hardware first-gesture take keeps the production 100 ms quiet — `setDeviceWait(0)` starts the take between the two applies, which is the take `di0frj` exists to prevent, and produced an empty clip in full-suite runs.

**Producer path unchanged.** An open session still starts a take synchronously (`lastDeviceChangeMs` empty, list built). Stop and Play still cancel a waiting one (`stopPlayback` / `startPlayback` still stop `takeStarter`).

**Numbers.** `ctest --preset linux-debug`: 71/71 in 67.38 s (was 65/65 in 90.59 s). The seven cases that spent 46.07 s waiting:

```
one Action from every domain lands while the transport rolls    10.33 → 5.32
one call to start playback is enough … survives device rebuild   8.46 → 0.97
a stopped transport is not asked to play again                   8.46 → 0.92
a take started as the first transport gesture … the rebuild      8.28 → 1.61
a headless session plays through the one-time device rebuild     4.47 → 0.50
an undo during a take neither stops it nor moves the playhead    3.14 → 3.03
a take waiting for the engine's devices is stopped by a Stop     2.93 → 1.92
                                                        was     46.07
                                                        now     14.27
```

31.80 s of that window reclaimed. Six new CI cases add about 9 s of session setup. Net 23.21 s faster.

`docs/ENGINE_NOTES.md` hazard 6 and `docs/ARCHITECTURE.md` record the seam.
