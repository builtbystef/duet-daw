---
id: vhl9d0
title: Assert audibility on the playback path, not only the offline render
state: todo
priority: high
labels:
    - maintenance
parent: b1j3me
created: 2026-08-19T03:34:22Z
updated: 2026-08-19T03:54:49Z
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
