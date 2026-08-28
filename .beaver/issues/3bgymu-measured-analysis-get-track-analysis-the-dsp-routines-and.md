---
id: 3bgymu
title: 'Measured analysis: get_track_analysis, the DSP routines, and the per-track cache'
state: in-progress
priority: medium
labels:
    - needs-review
depends_on:
    - v5yhh1
    - 6zog6s
parent: js437t
created: 2026-08-12T04:02:00Z
updated: 2026-08-28T10:49:39Z
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

## Notes

**claude** — 2026-08-28T10:49:39Z

Built 2026-08-28. Nine of the ten acceptance criteria are met and asserted; one
clause of the tenth is not, and cannot be from inside this slice. **This issue is
not closed** — see WHAT IS NOT MET at the bottom, which is the one thing that
needs a decision.

## WHAT LANDED

- `modules/duet_collab/include/duet/collab/Analysis.h` and `src/Analysis.cpp`,
  `Loudness.cpp`, `Spectrum.cpp`, `Onsets.cpp`, `Pitch.cpp`, `Fft.h` — a third
  target, `duet::collab_analysis`: `Waveform` and every tier-2 routine as a pure
  function of one. It links **nothing at all** — not the engine, not JUCE, not
  even the JSON the seam speaks — which is both what keeps it away from the
  audio callback structurally and what makes the spec's second seam a plain
  function call.
- `include/duet/collab/TrackAnalysis.h` + `src/TrackAnalysis.cpp` in
  `duet::collab_tools` — `get_track_analysis`, the per-track render cache, and
  `offlineTrackRenderer`, the render path as a function. `JsonRpc.h` gains
  `rpcError::runAbandoned` (-32004); `CollaboratorService::isRunInProgress` is
  now public, which is what a tool call that takes seconds asks.
- `duet_model` grew what a measurement needs: `readAudioFile` in a new
  `duet/model/AudioFile.h` (this being the only module that can open one),
  `renderSampleRate` / `renderBlockSize` / `renderBlockSeconds` in the public
  header, a `keepGoing` predicate on both renders, and `trackStateDigest`.
- `sidecar/src/vocabulary.ts` — the band edges in the tool's description are now
  stated in plain hertz throughout, and each band sits on one source line, which
  is what lets a test hold them to the routine's own table.
- `tests/AnalysisRoutinesTests.cpp` (26 cases) and `tests/TrackAnalysisTests.cpp`
  (8 cases); `ProjectToolsHarness.h` gains `ToolRunOptions`, which lends a run a
  `TrackAnalysis` the test owns and runs work on the message thread while a call
  is in flight. Two engine facts recorded in `docs/ENGINE_NOTES.md`.

## DECISIONS A REVIEWER NEEDS

1. **Three targets in one module now.** The routines link nothing, the tools
   link the model, the service links neither. A routine that cannot name an
   audio object cannot share a lock with the audio callback, and that is the
   whole of the real-time argument for the analysis layer — the same argument
   `duet_collab` already makes, one level further in.

2. **YIN is a routine and not a field.** The contract table in js437t has no
   pitch in `get_track_analysis`, and the issue's own criteria ask YIN to
   measure 440 Hz within a cent. Both are honoured: `analysis::pitchHz` exists,
   is tested, and is not in the tool's result. It is tier 3's — `notes` in
   `estimate_audio_content` — and this is where the routine it will use lives.

3. **The K-weighting is derived, not copied.** BS.1770-4 prints its coefficients
   for 48 kHz alone and Duet renders at 44.1. `Loudness.cpp` holds the design
   behind them — a high shelf and an RLB high-pass, each by corner, Q and gain —
   which at 48 kHz reproduces the standard's printed coefficients to every digit
   they are printed with. That is what says the design is the one the standard
   means. EBU Tech 3341 cases 1 through 5 are asserted within ±0.1 LU.

4. **A stereo tone's loudness is the level it was written at.** BS.1770 adds its
   channels rather than averaging them, and its −0.691 offset is the
   K-weighting's own gain at 1 kHz turned around. So the −6.02 dBFS tone the
   fixture renders measures −6.02 LUFS and −9.03 by RMS, and the two seams
   cross-check each other on it. The first version of the tool test expected the
   RMS figure and was wrong.

5. **The band power is normalised by the window's power, not its sum.** A tone
   spreads over the few bins of the window's main lobe, so what has to come out
   right is the total across them rather than the tallest of them. Getting it
   wrong reads every band 3.01 dB loud, which is exactly what the first version
   did. Blackman-Harris at 16384 points: a tone in the middle of any of the
   seven bands leaves at least 40 dB less in every other, sub included, which is
   the band the window length is chosen for.

6. **Onsets are two stages, and the first frame counts.** Spectral flux picks
   the frame, the waveform itself is then read inside that frame for the step
   its level rose most, which places an onset to within 1.5 ms rather than
   within a window. The first frame is a candidate like any other — what came
   before a render is silence — so a kick on bar 1 beat 1 is an onset. That is
   only true of a whole render, which is why the tool measures onsets over the
   whole render and *then* reads the range: a note held across the start of a
   bar range did not begin there.

7. **The cache holds the render, not the numbers, and is keyed per track.** The
   spec says cached per track; a call with a bar range is still a call about
   that track, so what is kept is the file the track rendered to and the range
   is measured out of it. Bounded memory whatever a project's length, and a
   second call for any range answers without rendering. The renderer is
   injected rather than called through the session, because "answers from cache
   without re-rendering" is a statement about that collaborator and there is no
   other way to observe it.

8. **A render leaves two properties on every track it rendered**, and the digest
   has to forget them or nothing is ever answered from a cache twice. Recorded
   in `docs/ENGINE_NOTES.md` with the probe that found it. The digest also
   carries the tempo map, because a track's own state says where a clip is in
   beats and the tempo is what turns that into a moment; the master's digest is
   the whole project's, because the master is the whole project.

9. **Cancellation is asked three times and answered `runAbandoned`.** Before the
   render, between the render's blocks — `Session::renderToFile` and
   `renderTrackToFile` take a `keepGoing` predicate now, and a render that says
   no stops and deletes what it half-wrote — and between the routines. What
   makes it work in production is `CollaboratorService::isRunInProgress`, which
   `cancelRun` turns false under the service's own lock while the service thread
   is still inside the tool; `activeRunId` cannot answer this, because a busy
   service thread has not processed the cancel yet.

10. **The band edges the model is told are checked against the routine's table.**
    The description that reaches the model is the sidecar's, so the test reads
    `sidecar/src/vocabulary.ts`, collapses the quotes, pluses and line breaks a
    long TypeScript literal is written with, and requires each band's own
    sentence in it. That is why the description's wording changed.

## CHECKS

- Format: `clang-format-18 --dry-run --Werror` over `git ls-files` — clean.
- Lint: `./scripts/lint.sh`, full sweep — clean.
- Build: `cmake --build --preset linux-debug -j 4` — clean.
- Test: `ctest --preset linux-debug` — 429/429, of which 34 are this slice's.
  One unrelated flake was seen once on the way (`the playhead is drawn where the
  transport is`, a GUI case that waits on real playback); it passed on its own
  and in two full runs after.

## WHAT IS NOT MET, AND WHY

The last criterion reads: "Rendering and analysis run on worker threads:
playback and editing continue while a first call is in flight, no analysis code
takes a lock the audio callback can take, and the run stays cancelable
throughout."

Three of those four hold and are asserted. **Playback does not continue.** An
offline render puts the Edit into the engine's own render status and the
transport stops for as long as the render runs — measured on the dev machine by
a `duet_scratch` probe that played, rendered on a worker thread, and sampled
`isPlaying()` from the message thread every five milliseconds throughout: 131
samples, none of them rolling. It is not Duet's `stopAllTransports` call beside
it; taking that line out changes nothing.

What happens instead is that playback comes *back*: `Session::startPlayback`
keeps asking a stopped transport to roll for ten seconds (hazard 6's remedy), so
a render shorter than that reads as a gap and a longer one leaves the transport
stopped with nothing asking it to roll again.

Published as **xbh9qk** (bug, high), with the probe, the three shapes a fix could
take — render a detached copy, defer the render until the transport stops, or
accept the gap and put playback back — and the note that whichever is chosen
changes a sentence of spec js437t. Fixing it means either a second Edit or a
change to what the spec promises, and neither is this issue's to decide.

Everything in this slice is written so that a fix drops in without touching it:
the render path is a `TrackRenderer` function, and where that render comes from
is the one thing xbh9qk would change.

`tests/TrackAnalysisTests.cpp` states the truth rather than the criterion: "a
measurement leaves the transport where it found it", with the reason and the
issue number in the case.

## WHAT THE USER MUST DO

Decide whether this slice is done. Either close this issue to accept it with
xbh9qk carrying the playback gap, or note the changes you want and remove
`needs-review`.
