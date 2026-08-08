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
updated: 2026-08-08T01:29:47Z
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
