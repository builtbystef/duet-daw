---
id: bmrxnw
title: Polyphonic transcription and instrument identity via Basic Pitch
state: done
assignee: claude
priority: low
depends_on:
    - 2z0y5u
    - lxt41c
parent: js437t
created: 2026-08-12T04:02:37Z
updated: 2026-08-30T06:32:08Z
---

## What to build

The remaining aspects of the estimating tool for audio tracks: the notes an audio clip contains, and what instrument it is. Transcription is Basic Pitch on ONNX Runtime with RTNeural — the analysis layer's one ML dependency, and the spec makes it explicitly separable: if the runtime causes build trouble it is dropped, and the rest of the Collaborator must not notice. Everything this produces is wrapped and taints the run's estimate ledger, exactly as key and chords do.

Any attribution or notice obligation the weights' licence carries is honoured here.

## Acceptance criteria

- [ ] Worked: an audio render of a C-major triad held for two beats returns wrapped notes containing pitches 60, 64, and 67, each starting within a few milliseconds of beat 0 and lasting about two beats, with at most one spurious extra note.
- [ ] Worked: a rendered monophonic melody of four known pitches returns those four pitches, in order, with their approximate start times.
- [ ] Instrument identity returns a wrapped string with a confidence; a track of white noise returns a low confidence rather than a confident wrong answer.
- [ ] Every value this slice produces is wrapped and taints the run's ledger; none of it is ever bare.
- [ ] Transcription runs on a worker thread and is cached and invalidated on the same terms as the rest of the analysis layer.
- [ ] With the ML runtime disabled at build time, the module still builds and the DAW still runs: the notes and instrument aspects report themselves unavailable, and key and chords keep working unchanged.
- [ ] The attribution and notice obligations recorded by the weights' licence research are satisfied in the build and in the repository's notices.

## Notes

**claude** — 2026-08-17T04:12:26Z

Scope note (2026-08-17): this slice owns the ONNX Runtime + RTNeural build integration — the prebuilt-tarball IMPORTED target (never find_package; psmj4y verified the shipped config package is broken) and RTNeural via FetchContent with its default Eigen backend, all behind one CMake option (DUET_ENABLE_POLYPHONIC_TRANSCRIPTION) so u24m3x's escape hatch stays a -D flag. sea14w deliberately excludes both dependencies.

**claude** — 2026-08-28T21:17:48Z

2z0y5u built `estimate_audio_content` for key and chords, and narrowed what the
model may ask for to those two: the `aspects` union in `sidecar/src/vocabulary.ts`
lists `key` and `chords` alone, and a test in `tests/ContentEstimateTests.cpp`
("the aspects the model may ask for are the aspects this build estimates") holds
it to that. An aspect the model may ask for and never gets an answer to is worse
than one it was never offered.

So this issue adds `notes` and `instrument` back — to that union, to the tool's
own description, and to that test — beside the transcription that answers them.
`ContentEstimates::estimate` is where they land: an aspect nobody asked for is
absent from the result rather than empty, and so is one the routine could not
read, which is the shape these two want as well.

**claude** — 2026-08-30T06:32:08Z

Built (2026-08-30). `estimate_audio_content` answers `notes` and `instrument`
from Spotify's Basic Pitch on ONNX Runtime; both are wrapped and both write to
the run's ledger, exactly as key and chords do.

## Where it lives

`duet::collab_transcription` is a target of its own — `Transcription.h`, plus
`BasicPitch.cpp` when the model is there and `NoBasicPitch.cpp` when it is not,
plus `Instrument.cpp`, which is compiled either way because what needs the
runtime is the notes and not the reading of them. One option,
`DUET_ENABLE_POLYPHONIC_TRANSCRIPTION` (default ON), and
`transcription::available()` is what every caller asks.

## RTNeural is not there, and that is the one thing this slice did against the
## 2026-08-17 scope note

That note called for ONNX Runtime *and* RTNeural, following NeuralNote.
NeuralNote needs both because it re-implemented Basic Pitch's network in RTNeural
and hand-wrote the constant-Q transform in front of it. Spotify's own ONNX
serialisation needs neither: the graph *begins* with the CQT (`cq_t2010v2` is in
the file), so the runtime is handed raw 22050 Hz audio and hands back the three
posteriorgrams. A second inference library that nothing runs is weight without
work, so the build declares one dependency and not two. Everything else the scope
note asked for stands: prebuilt tarball, IMPORTED target, never `find_package`.

## What the C++ does

Sum to mono; resample to 22050 Hz through a Blackman-windowed sinc, because
dropping every other sample of a 44.1 kHz render would fold everything above
11 kHz onto the pitches being read; window at 43844 samples with 30 frames of
overlap; one call to the model per window; drop 15 frames from each end of each
answer; then Basic Pitch's own note-creation algorithm at its published defaults
(onset 0.5, frame 0.3, an 11-frame minimum and an 11-frame tolerance), melodia
trick included.

The three outputs are bound by name, and the mapping is not the one the tensor
order suggests: `StatefulPartitionedCall:2` is **onset**, `:1` is **note**, `:0`
is the contour, which Duet does not read (it is what pitch bends are made of).
Getting those two the wrong way round yields a plausible-looking posteriorgram
with notes three frames long and no transcription at all — worth an hour, so it
is written down here. The load refuses any file whose tensors are not those:
the weights are pinned by SHA-256, so a mismatch is a wrong file rather than a
runtime condition.

## Instrument identity

There is no property of a waveform that says "bass", so this is a documented
reading of the transcribed notes: their register, how much of the stretch they
cover, and how many sound at once, naming one of four families — drums or
percussion, bass, keys or another chordal instrument, lead or melody instrument.
The confidence is that fit multiplied by `1 - spectralFlatness`, and that last
factor is the whole of what keeps white noise from being named confidently: it
looks percussive to anything that only counts notes, and a spectrum with no shape
at all is evidence for nothing.

## Aspects, and what a build without the model says

All four literals are back in `sidecar/src/vocabulary.ts` and in the tool's
description, and the test that holds the union to what this build estimates
asserts all four. A build without the model turns a call that asked *only* for
`notes` or `instrument` away with an invalid-params error naming what it can
estimate — an answer, not a silence — and leaves them absent from a call that
asked for other things too, which is the same shape as an aspect that could not
be read. The vocabulary is one static TS file and the option is a CMake flag, so
the alternative was a shipped schema that lies about a build; an explicit refusal
was judged the better of the two.

## Seams

Spec seam 2 (routines as pure functions), `tests/TranscriptionRoutinesTests.cpp`:
a C major triad held one second — two beats at 120 — comes back as exactly 60,
64 and 67, each starting inside 50 ms and lasting 0.94 s, with no spurious note
at all; a four-note line comes back as its four pitches at 0.0, 0.5, 1.0 and
1.5 s; noise comes back named with a confidence under 0.2 and below the chord's.
Spec seam 1 (the socket), `ContentEstimateTests.cpp`: wrapped, in the project's
own beats, velocity 1..127, four ledger lines for a call that asked for
everything, and a white-noise track answered unconfidently. Every case says which
build it is in, so the suite is the same suite either way.

Fifty milliseconds rather than "a few": the model answers in frames of 256
samples at 22050 Hz, so 11.6 ms is its resolution and two frames is its floor.

## Verified both ways

With the option ON: `ctest` 601/601, one skipped — the case that asserts the
build without the model. With it OFF: the whole tree builds, the app builds, and
the collab suite is 120 passed and 6 skipped, every key and chord case among the
passes. The DAW was started and left running for twenty seconds in each build.

## Licence

The weights are fetched by URL and SHA-256 rather than vendored, so the one
artifact under someone else's licence stays separately packaged, which is what
lxt41c asked for. `THIRD_PARTY_NOTICES.md` carries the Basic Pitch NOTICE and the
ONNX Runtime MIT text, `licences/Apache-2.0.txt` the licence itself, and the
app's POST_BUILD puts both beside the binary with `nmp.onnx` and
`libonnxruntime.so.1`. Duet ships the model byte-for-byte, so no changed-file
notice under Apache-2.0 §4(b) applies.

## Not verified here

The three sanitizer configurations. ONNX Runtime is a prebuilt, uninstrumented
shared library. The session is opened with one intra-op thread, one inter-op
thread and sequential execution, so it starts no threads of its own, but a TSan
or ASan report from inside it would be a fact about the nightly rather than about
this code.

The full lint sweep is clean but for `PluginScanDialog.cpp:156`, which this slice
does not touch and which issue 1qdjq5 already holds.
