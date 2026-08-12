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
updated: 2026-08-12T04:02:37Z
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
