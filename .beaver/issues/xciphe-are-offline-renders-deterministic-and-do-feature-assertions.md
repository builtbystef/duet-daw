---
id: xciphe
title: Are offline renders deterministic, and do feature assertions prove built-in DSP correct?
state: done
assignee: agent
priority: medium
labels:
    - roadmap:d9gioe
    - session:prototype
depends_on:
    - 86t5lu
parent: d9gioe
created: 2026-08-10T23:44:17Z
updated: 2026-08-11T00:59:41Z
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

## Notes

**agent** — 2026-08-11T00:59:41Z

## Accepted verdict — 2026-08-10

Offline Tracktion renders are bit-exact under fixed parameters on the tested GCC 13/Linux host: the same Edit rendered twice had 0 differing samples and maximum absolute difference 0. The diagnostic fingerprint 12856307083160462901 repeated across five independent Debug processes and the Release build. This does not prove identity across machines, CPUs, compilers, or operating systems, so raw sample equality, fingerprints, and golden renders are not product test contracts.

Feature assertions through the public processing seam are viable. A Duet-owned stand-in JUCE instrument rendered known MIDI as A4 = 439.985 Hz and C5 = 523.243 Hz. Requested onsets at 0.25 s and 1.0 s measured 0.243832 s and 0.998481 s: Tracktion places note-ons at the start of their containing 512-sample render block, so onset assertions allow up to one render block early. A synthetic 440 Hz sine rendered through a Duet-owned stand-in JUCE gain effect measured RMS 0.353553 dry and 0.176777 wet, exactly -6.0206 dB.

Transfer pattern: construct the Edit from synthetic reference material; render to a fresh temporary file with fixed sample rate, block size, bit depth, and dithering disabled; load an AudioBuffer; assert measured musical/audio features with domain tolerances. Instrument tests insert known MIDI and measure pitch/onset. Effect tests render the same source before and after the processor and measure RMS or a spectral-bin ratio. Analysis DSP tests likewise use synthetic references whose ground truth is known by construction. No production analysis DSP exists yet, so this spike used test-owned rising-zero-crossing pitch, amplitude-threshold onset, and RMS measurement.

Traps: use a fresh destination per render to avoid Tracktion AudioFile cache effects; Catch2 3.15 discovery writes under XDG_RUNTIME_DIR, which is read-only in this sandbox, so the spike uses one explicit add_test; disabling automatic audio-device initialisation still leaves a non-fatal ALSA MIDI enumeration when /dev/snd/seq is absent; the transport play-retry trap does not apply because Renderer::RenderTask drives the graph directly.

Prototype: branch prototype/offline-render-correctness, commit 7241427. Debug and Release pass all 16 assertions; Debug also passed ten consecutive CTest runs. clang-format was unavailable on the host, so the format check could not run.

Reason: these results prove the audio-correctness doctrine at the seam Duet controls. Stable within-host samples are useful diagnostic evidence, while synthetic feature assertions state the behavior that must survive platform and implementation changes.
