---
id: 6zog6s
title: 'Offline render harness: feature assertions and the determinism canary'
state: done
assignee: claude
priority: medium
depends_on:
    - 4r7nlj
parent: b1j3me
created: 2026-08-11T01:51:58Z
updated: 2026-08-25T17:12:48Z
---

## What to build

The audio-correctness test harness per spec b1j3me / ADR 0006 and branch prototype/offline-render-correctness (16/16): headless offline rendering of an Edit through the engine's Renderer on a worker thread, plus feature-assertion helpers — measured pitch, onset positions, RMS/spectral change — with domain tolerances. Never golden files, fingerprints, or stored-sample comparison. Renders go to a fresh destination every time (the engine caches by destination file), at fixed sample rate, block size, and bit depth, with dithering off. The one permitted sample comparison is the within-process determinism canary. This harness is also the per-track render path js437t's tier-2 analysis will call.

## Acceptance criteria

- [ ] A whole-Edit render and a single-track (isolated from a multi-track Edit) render both produce a readable audio file headlessly, off the message thread, with no audio device.
- [ ] Worked example: an Edit with a 440 Hz sine clip spanning 1.0s–2.0s renders to: RMS below the silence threshold before 1.0s, a detected onset no more than one render block before 1.0s and not after it (the engine places note-ons at the start of their containing block), and measured pitch of 440 Hz within the helper's stated tolerance during the clip.
- [ ] A MIDI note through a built-in instrument yields a detected onset within the same one-block-early tolerance of its scheduled start.
- [ ] An RMS/spectral-change assertion distinguishes a real change (a gain drop of 6 dB mid-render) from an unchanged render.
- [ ] Determinism canary in the ordinary suite: the same Edit rendered twice in one process produces byte-identical output; the comparison is never against a stored file.
- [ ] Two consecutive renders of different Edit states produce different output (fresh destination per render — no stale cache result).
- [ ] The suite contains no golden files and no stored audio fingerprints; reference signals are synthetic (ground truth by construction).

## Notes

**claude** — 2026-08-25T17:12:48Z

Done. The harness is `duet::testing::Render` plus `renderProject`/`renderTrack`
(tests/support), over two model renders (`Session::renderToFile`, the existing
one rewritten, and `Session::renderTrackToFile`, new). Eight cases in
tests/OfflineRenderTests.cpp, one per criterion. Full suite 279/279; format,
lint and build clean.

**The render.** The engine's own `Renderer::renderToFile` takes the rate and the
block size from the audio device — which a headless machine has none of — and
drives the task through a progress window, so the model now builds
`Renderer::Parameters` itself: 44100 Hz, 512-sample blocks, 32 bits, dithering
off, and a destination deleted before every render so the audio-file cache
cannot answer with the last one. The engine's guards are kept, and the per-track
render leaves the master chain out, because what a track puts out on its own is
what analysis will measure.

**Off the message thread.** Not a matter of taste: the render's guards and its
graph are the message thread's work and the engine asserts as much, while the
blocks are not. `renderTracksToFile` marshals setup and teardown through
`callBlockingCatching` and drives the block loop where it was called, so it is
callable from a worker thread while the message loop runs — which is what
js437t's tier-2 analysis needs and what the harness does for every render
(`Render::ranOffTheMessageThread`).

**One deviation from the criteria, with a reason.** The canary compares samples
bit for bit, not file bytes. The engine stamps `Time::getCurrentTime()` into
every rendered file's broadcast-wave header, so two renders of one edit a second
apart differ in exactly one byte at offset 441 and in none of the 706 000 that
follow; two inside the same second are identical throughout. The audio is the
claim ADR 0006 is making, and the header is a clock.

**Measurements**, all test-owned and deliberately simple — the production
analysis DSP is 3bgymu's, and nothing here is a step towards it: peak, RMS and
silence; pitch from rising zero crossings; onsets; level change between two
stretches; and one frequency's level by Goertzel, which is the spectral half the
overall level cannot answer. The onset walks back from the level crossing to
where the sound left silence, a few milliseconds at a time so a tone's own zero
crossings are not mistaken for the silence in front of it, and gives up after one
block. Without that walk-back every onset reads late by however long the sound
took to rise, which is the one thing an onset measurement may not do.

**Measured on the dev machine**: 440 Hz clip at 1.0 s → onset 1.000000, pitch
440.01 Hz, silence before it exactly zero. A4 through 4OSC → onset 0.999977 to
1.000000, pitch 440.01 Hz. A 6 dB automation drop at half time → -6.00 dB by RMS
and -6.00 dB at 440 Hz; the same render without it, 0.00 dB by both. One track
of a two-track project rendered alone → its own tone at -6.02 dB and the other
track's below -100 dB.

**Three engine facts recorded** in docs/ENGINE_NOTES.md, each cited by the code
that works around it: `toBitSet` answers with every track whatever it is asked
about (which is why the per-track bit is set by hand); a render's setup and
teardown are the message thread's and its blocks are not; every rendered file
carries the wall clock in its header.

**Discovered, not done here**: a whole-project render ignores mute and solo — the
engine's solo isolator unmutes every track it is given, and for a whole-project
render that is all of them. Published as sh2dkg (bug), with zm174o's
Export/Bounce made to depend on it. The isolation the per-track render needs is
that same guard doing its job, so it stays where it is.

ARCHITECTURE.md names the render path under `duet_model`.
