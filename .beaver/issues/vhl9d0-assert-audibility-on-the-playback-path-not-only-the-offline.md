---
id: vhl9d0
title: Assert audibility on the playback path, not only the offline render
state: done
assignee: claude
priority: high
labels:
    - maintenance
parent: b1j3me
created: 2026-08-19T03:34:22Z
updated: 2026-08-19T06:14:58Z
---

## Why

A routing bug shipped through a green suite: `createTrack(TrackKind::group, ...)` gave the bus no output, and the playback graph wraps a track with no output device and no destination track in a `SinkNode`, whose own documentation is "Blocks audio and MIDI input from reaching the outputs". Anything routed into such a bus is digitally silent — measured at exactly 0.0 RMS on the device — and every test passed anyway.

The reason none of them caught it: the engine builds two different graphs. `createNodeForEdit (EditPlaybackContext&, ...)` is the playback path and is the one that adds the `SinkNode`; `createNodeForEdit (Edit&, ...)` is the offline-render path and instead sums a no-output track straight into the master. ADR 0006 chose measured features of an offline render as the way Duet asserts audio, so every audio assertion Duet has is blind to the playback graph by construction.

## What to build

A facade read that measures what the playback graph actually produces — an output level for the master, and per track — so a test can play a project on a real device and assert that a signal reaches the output. Then a test that plays audio through a group bus and asserts it is heard, which is the case that shipped broken.

The measurement that proved the bug is reproducible without new code and is what this should replace:

```
pactl load-module module-null-sink sink_name=duetprobe
ID=$(pactl list short sinks | awk '/duetprobe/{print $1}')
pw-record --target "$ID" --rate 48000 --channels 2 out.wav &   # by node id — a name target silently falls back to the default source
pw-jack ./build/tests/Debug/duet_tests "<the case>" &
sleep 4 && for ch in FL FR; do pw-link "alsa_playback.duet_tests:output_$ch" "duetprobe:playback_$ch"; done
```

It needs pw-record, pw-link and a null sink, so it cannot be a CI assertion as it stands. Deciding what part of it can be — and whether ADR 0006 needs an amendment saying the offline render does not stand in for the playback graph — is part of this issue.

## Acceptance criteria

- [ ] The facade can report the level reaching the output, on the playback path, without naming an engine type.
- [ ] A test plays a project whose only source is routed through a group bus and asserts a non-zero level; it fails against the pre-fix `setOutputToNone` behaviour.
- [ ] Either the test runs in CI, or the issue records why it cannot and what does run instead.
- [ ] ADR 0006 is amended, or a note records why it needs no amendment.

## Notes

**claude** — 2026-08-19T03:54:49Z

Second instance of the same gap, from the same review pass: the demo clipped the master — 5,425 samples pinned at full scale — and nothing in the suite could see it, for the same reason nothing could see the silent bus. A level read on the playback path should be able to answer 'is it clipping' as well as 'is it audible', so the acceptance criteria here are worth reading as covering both: a peak/true-peak read, not just a non-zero one.

**claude** — 2026-08-19T06:14:40Z

Done. The playback graph can now be asked what it is putting out, and the group bus that shipped silent is a test.

**What the facade gained** (`duet::model::Session`, all plain doubles, no engine type):

- `outputPeakDb()` — the loudest the output has been since the last read, in decibels of full scale. Fed by `EditPlaybackContext::masterLevels`, which only the playback graph writes to (`EditNodeBuilder.cpp`, the `LevelMeasuringNode` wrapped around the default wave output device after the master plugins) — so this is the one number in Duet that an offline render cannot produce.
- `trackPeakDb(TrackRef)` — the same read per track, off the engine's own `LevelMeterPlugin`, which sits after the track's fader. A track reading loud while the output reads silent is the signature of a track routed into something nobody can hear.
- `playWithoutAudioDevice(seconds)` — runs the playback graph block by block with no audio hardware, as fast as the machine goes, and stops. `silentDb` (−100) is what a meter reads with no signal at all.

**The decision (criterion 3): the assertion runs in CI, and does not merely skip there.** The bug went through the push gate, so a test that skips on the runner would not have caught it either. The engine's own headless test player (`tracktion_EnginePlayer.h`) drives playback through `HostedAudioDeviceInterface`, and that is what `playWithoutAudioDevice` does: switch the device manager to the hosted device, then hand it blocks. It is the same `createNodeForEdit(EditPlaybackContext&, ...)` graph — `SinkNode` and all — so it sees exactly what the pw-record measurement in this issue's body saw, in about a second and with nothing to install. Verified as a runner would run it: with the environment stripped so no audio device can be found (`env -i HOME=… PATH=… ALSA_CONFIG_PATH=/dev/null`), the suite is 51 passed / 6 skipped / 0 failed, and both new device-free cases are among the 51. On the dev machine with its device, 57/57.

**The red proof (criterion 2).** With `setOutputToNone` put back into `createTrack` for a group: track −8.09 dB, bus −8.09 dB, **output −100 dB** — the assertion fails on the output alone, which is the whole diagnostic. With it removed: track −8.09, bus −8.09, output −11.09 dB.

**Tests** (`tests/PlaybackLevelTests.cpp`), all at the model seam:

- "a track routed through a group bus is heard at the output" — the demo phrase routed into a group bus; track, bus and output all above −40 dB. Device-free, so it runs in CI.
- "the output says when a project has run out of headroom" — the phrase as played peaks at −11.1 dB and below 0; piling gain up (fader to its limit, then the same signal again through a send into a bus) puts the output at +8.85 dB, at or above full scale. This is the second note's ask: the read answers 'is it clipping' as well as 'is it audible'.
- "a project played through a real audio device is heard at the output" — the same read on real hardware, skipped where there is none. It measured −11.0994 dB through PipeWire, matching the device-free reading to within a hundredth of a decibel.

**Facts a reviewer needs:**

- A `LevelMeasurer` measures nothing while no client is listening to it, and the master's belongs to the playback context, which every graph rebuild frees and remakes. So the meters are attached in `askTransportToPlay` (right after the context is allocated, before audio can reach it) and re-pointed on every `keepPlaybackRolling` tick — the same shape the engine's own `ExternalControllerManager` uses for its 50 Hz master meter. Track meters are synced by the same call and dropped when their track goes.
- Clipping is read off the peak and not off the engine's overload flag: `LevelMeasurer::Client::getAndClearOverload()` does not report an overload — it reports whether the caller should clear its indicator — and the per-channel overload array has no accessor. Full scale is 0 dB, and the measuring node sits before the device's channel conversion, so a signal over full scale reads over 0.
- `setTrackVolumeDb` clamps at +6 dB, which is the engine's fader range; that is why the headroom case piles gain through a send instead of asking for +24 dB.
- A session that has played without a device has given up its audio device to do it and keeps playing without one. Said so in the header.
- The demo phrase does not clip on the playback path over its first second (−11.1 dB). The clipping this issue's second note reports is therefore not in the phrase as played from its start; 6629zo's hanging voices remain the suspect, and `outputPeakDb` is now what can measure it.

**Criterion 4:** ADR 0006 is amended — a measured offline render does not stand in for the playback graph, and audibility and headroom are asserted on the playback graph through these meters. `docs/ARCHITECTURE.md` and the CI workflow's comment about audio hardware say the same.

Checks: format, lint, and 57/57 ctest green.

To confirm by ear: nothing changed about what plays. To confirm by eye: `./build/tests/Debug/duet_tests "a track routed through a group bus is heard at the output" -s` prints the three levels.
