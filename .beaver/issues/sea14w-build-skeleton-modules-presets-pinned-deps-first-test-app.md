---
id: sea14w
title: 'Build skeleton: modules, presets, pinned deps, first test, app plays audio'
state: todo
priority: high
parent: b1j3me
created: 2026-08-11T01:49:57Z
updated: 2026-08-11T01:49:57Z
---

## What to build

The repository becomes a buildable Linux application with the settled toolchain (spec b1j3me, node psmj4y as corrected by ddp1qt). Three module targets exist — the model facade, the persistence facade, and the GUI application shell — plus a Catch2 test suite. The app, launched via pw-jack, opens a window, initializes the audio device, and plays audible audio through the engine transport. This is the walking skeleton (branch prototype/walking-skeleton) promoted to product code; the documented check commands in AGENTS.md work from a fresh clone.

## Acceptance criteria

- [ ] `cmake --preset linux-debug` configures with Ninja Multi-Config from a checked-in CMakePresets.json (schema v3) and exports compile_commands.json; `cmake --build --preset linux-debug` builds (locally `-j 4`).
- [ ] The project declares a VERSION; C++20 is set by hand on every Duet target; `atomic` is linked explicitly.
- [ ] Dependencies come in by FetchContent with full commit-SHA pins: JUCE 9 declared before Tracktion Engine (`develop`), Catch2 v3; `TE_ADD_EXAMPLES` off; flags `JUCE_JACK=1`, `TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH=1`, `JUCE_USE_CURL=0`, `JUCE_WEB_BROWSER=0`, `JUCE_PLUGINHOST_LADSPA=0`.
- [ ] Module targets exist for the model facade, the persistence facade, and the GUI app shell; the two facades' public headers compile without engine or JUCE include paths (the engine-free rule holds from day one, even while the facades are near-empty).
- [ ] `ctest --preset linux-debug` runs a Catch2 suite with at least one real headless test (an in-memory engine Edit is created and inspected).
- [ ] Launched via pw-jack on the dev machine, the app opens a window, initializes the audio device, and plays audible audio through the engine transport, started and stopped from the shell.
- [ ] `clang-format` and `clang-tidy -p build/` run clean over the new Duet sources.
