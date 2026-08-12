---
id: 3bgymu
title: 'Measured analysis: get_track_analysis, the DSP routines, and the per-track cache'
state: todo
priority: medium
depends_on:
    - v5yhh1
    - 6zog6s
parent: js437t
created: 2026-08-12T04:02:00Z
updated: 2026-08-12T04:02:00Z
---

## What to build

Tier 2 of the analysis layer: hand-written DSP over a track's rendered output, exposed as one tool over a whole track or a bar range. The routines are peak, true peak, RMS, integrated and short-term LUFS per ITU-R BS.1770, crest factor, energy in the fixed named spectral bands (sub, low, low-mid, mid, high-mid, high, air — whose edges the tool's own description documents to the model), spectral centroid, spectral flatness, stereo correlation and width, onsets via spectral flux, and monophonic f0 via YIN. Everything here is measured by a documented routine, so everything crosses the seam as a bare scalar.

The routines are pure functions of a waveform — that is the test seam, and reference signals are synthetic, so ground truth is true by construction. Analysis input is the track's rendered output through the offline render path. Computation is on demand on a worker thread, cached per track and keyed on that track's edit state, invalidated by any edit that changes that track's output. A multi-second first call is acceptable; playing, editing, and recording continue throughout.

## Acceptance criteria

- [ ] A full-scale 1 kHz sine measures peak 0 dBFS, RMS −3.01 dB, and crest factor 3.01 dB, each within the routine's stated tolerance; a full-scale square wave measures crest factor 0 dB.
- [ ] EBU Tech 3341 conformance: the published test signals measure their published values within ±0.1 LU, including the −23.0 LUFS reference cases, for both integrated and short-term maximum loudness.
- [ ] YIN: a 440 Hz sine measures 440 Hz within ±1 cent; silence reports no pitch rather than a wrong one.
- [ ] Onsets: a rendered click pattern at known beat positions yields onsets within a few milliseconds of those positions, and reports no onsets that are not there.
- [ ] Stereo: an identical-both-channels signal measures correlation 1.0; a channel against its own inversion measures −1.0; a mono-summed signal measures zero width.
- [ ] Spectral bands: a sine placed inside one band puts its energy in that band and at least 40 dB less in every other, and the band edges the model is told match the edges the routine uses.
- [ ] The tool returns every field of the contract as a bare scalar — nothing it produces is ever wrapped as an estimate.
- [ ] Bar range, worked: a track silent before bar 5 measures as silence over bars 1–4 and as signal over bars 5–8; omitting the range measures the whole track.
- [ ] The second call for an unchanged track answers from cache without re-rendering; an edit to that track invalidates it and the next call re-renders; an edit to a different track does not invalidate it.
- [ ] Rendering and analysis run on worker threads: playback and editing continue while a first call is in flight, no analysis code takes a lock the audio callback can take, and the run stays cancelable throughout.
