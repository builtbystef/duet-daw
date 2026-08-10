---
id: xciphe
title: Are offline renders deterministic, and do feature assertions prove built-in DSP correct?
state: todo
priority: medium
labels:
    - roadmap:d9gioe
    - session:prototype
depends_on:
    - 86t5lu
parent: d9gioe
created: 2026-08-10T23:44:17Z
updated: 2026-08-10T23:44:17Z
---

The audio-correctness doctrine settled at the Frontier-sharpening session (2026-08-10): tests render offline and assert **measured features** ("compressor reduces crest factor", "EQ cuts 6 dB at 1 kHz"), never golden-file comparison (brittle across platforms/compilers). The doctrine rests on a fact nobody has verified: **is an offline render of a Tracktion Edit deterministic on this engine** — same Edit, same output, run to run and machine to machine (at least within the CI image)?

Prototype (disposable code, headless, Catch2 harness like the skb4tp/rquzdc spikes):

- Render a small Edit offline twice; compare outputs. Establish what "deterministic" holds: bit-exact, or within a tolerance — and if tolerance, what a sane epsilon is.
- Prove **one instrument test end to end**: feed the built-in synth (or a stand-in JUCE processor if the synth does not exist yet) known MIDI, render, assert measured features (pitch of rendered note, note onset positions).
- Prove **one effect test end to end**: render a known signal through an EQ or gain stage, assert the measured spectral/level change.
- Feature measurement reuses the analysis DSP where it exists; where it does not, the test synthesizes its own measurement (a Goertzel bin, an RMS window) — record which was needed.

Also settled, and to be exercised here where cheap: the **analysis DSP itself validates against synthetic reference signals only** for milestone one — sines at known frequencies, EBU R128 reference signals with published LUFS values, clicks at known positions, rendered chord progressions; ground truth known by construction, zero licensing questions. Corpus benchmarking (GiantSteps and kin) stays on the Frontier until a routine disappoints in use.

Deliverable: the determinism verdict (and tolerance if any), the two proven test patterns ready to transfer into real Catch2 suites, and any traps found (cf. b1j3me's known trap: transport dies after first headless playback — retry play() until isPlaying).

Trust boundary (settled): Tracktion Engine is trusted; these tests target Duet's own instruments, effects, and analysis routines through their public seams.
