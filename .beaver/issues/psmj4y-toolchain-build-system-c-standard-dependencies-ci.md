---
id: psmj4y
title: 'Toolchain: build system, C++ standard, dependencies, CI'
state: todo
priority: medium
labels:
    - roadmap:d9gioe
    - session:research
depends_on:
    - 1hn16k
parent: d9gioe
created: 2026-08-07T06:02:31Z
updated: 2026-08-08T07:16:53Z
---

Research session, sized small. Given the chosen foundation (node 1hn16k): build system (CMake presumed — confirm what the foundation expects), C++ standard, dependency management (FetchContent / vcpkg / submodules), compiler/platform matrix from the milestone-one platform decision, and a minimal CI shape.

Deliverable: the concrete toolchain choices, ready to become an ADR and to close Beaver issue l1gtax ("Establish the checks when the C++ stack lands") when implemented.

## Notes

**claude** — 2026-08-07T06:27:08Z

Constraint from the user (2026-08-07): the project starts as open source, with the potential to become a commercial product later. Evaluate every candidate technology, library, and service against that path — prefer permissive or dual-licensable licenses; a copyleft-only option with no commercial-license route (e.g. GPL with no paid tier) forecloses a commercial edition and needs explicit justification. Recorded in the roadmap root (d9gioe) under 'Licensing posture'.

**claude** — 2026-08-08T01:29:47Z

Constraints from node lf8tnt (2026-08-07), which adopted Tracktion Engine as the engine layer:

- C++ STANDARD FLOOR RISES TO C++20. README: 'N.B. Tracktion Engine requires C++20'. This overrides the C++17 floor noted from JUCE at node 1hn16k.
- CMake: Tracktion declares 'cmake_minimum_required (VERSION 3.15...3.20)'.
- PIN A COMMIT, NOT A TAG. The default branch is 'develop', not 'master'. Last tag is v3.2.0 (2025-05-15) while VERSION.md on develop reads 3.5.0 — roughly 1.5 minor versions of unreleased work. 'master' last moved 2025-08-22 and is 15 months stale. The consumable branch is develop, so dependency management must pin an explicit commit for reproducibility.
- JUCE 9 COMPATIBILITY IS CI-PROVEN BUT NOT DECLARED. Tracktion's nightly 'juce_compatability' workflow builds against 'gh:juce-framework/JUCE#develop' (now 9.0.0) and was green on 2026-08-07. But the vendored submodule still pins a pre-9 JUCE (37c894f, 8.0.13), and no README/CMake statement mentions JUCE 9. Decide whether Duet uses Tracktion's vendored JUCE submodule or supplies its own JUCE 9 — this is a real toolchain choice, not a detail.
- Two JUCE options to set deliberately: JUCE_JACK (default 0; the Linux-first path is JACK-via-PipeWire) and the Signalsmith time-stretch flag (all four Tracktion stretch backends default to 0, and with none enabled time-stretch is disabled entirely — Signalsmith is bundled, MIT, header-only, no linking).
- juce_audio_devices declares 'linuxPackages: alsa' as its system dependency.

**claude** — 2026-08-08T07:16:53Z

Constraints from node u24m3x (2026-08-08), which settled the Collaborator's Tool Vocabulary:

- NEW ML DEPENDENCY IN THE BUILD MATRIX: polyphonic transcription ships in milestone one via Spotify's Basic Pitch, which means ONNX RUNTIME (MIT) plus RTNEURAL (BSD-3). NeuralNote (github.com/DamRsn/NeuralNote, Apache-2.0) is the proven JUCE C++ reference: it splits the Basic Pitch CNN into four sequential models for RTNeural and uses ONNX Runtime for the constant-Q + harmonic-stacking front end. ONNX Runtime is a substantial per-platform binary — how it is acquired and pinned (FetchContent vs vcpkg vs prebuilt) is a real toolchain decision, not a detail. The user has explicitly accepted dropping polyphonic transcription if this runtime causes build trouble, so keep it separable.
- NO GPL/AGPL ANALYSIS LIBRARIES. aubio (GPL-3), libKeyFinder (GPL-3), Gist (GPL-3) and Essentia (AGPL-3) are all ruled out on licence. There is no permissively-licensed C++ MIR library, so the DSP layer (loudness, spectrum, dynamics, key, monophonic pitch, onsets) is hand-written with no third-party MIR dependency at all.
- PLUGIN FORMAT NARROWED: milestone one hosts VST3 only; CLAP is deferred to milestone two. No CLAP SDK in the build.

Also relevant: ITU-R BS.1770 loudness must be implemented in-house (lf8tnt found Tracktion's level measurement is a runtime meter with no offline API).
