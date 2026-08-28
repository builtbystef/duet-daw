---
id: bmrxnw
title: Polyphonic transcription and instrument identity via Basic Pitch
state: todo
priority: low
depends_on:
    - 2z0y5u
    - lxt41c
parent: js437t
created: 2026-08-12T04:02:37Z
updated: 2026-08-28T21:17:48Z
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
