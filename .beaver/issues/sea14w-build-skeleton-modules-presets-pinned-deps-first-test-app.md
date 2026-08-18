---
id: sea14w
title: 'Build skeleton: modules, presets, pinned deps, first test, app plays audio'
state: done
assignee: claude
priority: high
parent: b1j3me
created: 2026-08-11T01:49:57Z
updated: 2026-08-18T07:12:40Z
---

## What to build

The repository becomes a buildable Linux application with the settled toolchain (spec b1j3me, node psmj4y as corrected by ddp1qt). Three module targets exist — the model facade, the persistence facade, and the GUI application shell — plus a Catch2 test suite. The app, launched via pw-jack, opens a window, initializes the audio device, and plays audible audio through the engine transport. This is the walking skeleton (branch prototype/walking-skeleton) promoted to product code; the documented check commands in AGENTS.md work from a fresh clone.

## Acceptance criteria

- [ ] `cmake --preset linux-debug` configures with Ninja Multi-Config from a checked-in CMakePresets.json (schema v3) and exports compile_commands.json; `cmake --build --preset linux-debug` builds (locally `-j 4`).
- [ ] The project declares a VERSION; C++20 is set by hand on every Duet target; `atomic` is linked explicitly.
- [ ] Dependencies come in by FetchContent with full commit-SHA pins, taken from the proven prototype branches: JUCE `f8f8864172464b9adf9eba6101e1f784838d1597` (tag 9.0.0) declared before Tracktion Engine `494e91d2ff546353b69723a5e992dd71d1a0204b` (develop 2026-08-03, `GIT_SUBMODULES ""`), Catch2 `8b08d4d79514f45f7e4ce2a607ac9c94e920d1bb` (tag 3.15.3); `TE_ADD_EXAMPLES` off; flags `JUCE_JACK=1`, `TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH=1`, `JUCE_USE_CURL=0`, `JUCE_WEB_BROWSER=0`, `JUCE_PLUGINHOST_LADSPA=0`.
- [ ] Module targets exist for the model facade, the persistence facade, and the GUI app shell; the two facades' public headers compile without engine or JUCE include paths (the engine-free rule holds from day one, even while the facades are near-empty).
- [ ] `.clang-format` and `.clang-tidy` are committed at the repository root and drive the format and lint checks — the l1gtax deliverables land here.
- [ ] `ctest --preset linux-debug` runs a Catch2 suite with at least one real headless test (an in-memory engine Edit is created and inspected).
- [ ] Launched via pw-jack on the dev machine, the app opens a window, initializes the audio device, and plays audible audio through the engine transport, started and stopped from the shell (manual check on the dev machine, recorded as a note on this issue — not an automated test).
- [ ] `clang-format` and `clang-tidy -p build/` run clean over the new Duet sources.

## Notes

**claude** — 2026-08-18T07:12:21Z

Completed 2026-08-18. The repository is a buildable Linux application: three module targets, a Catch2 suite, and an app that plays audio.

WHAT LANDED
- CMakeLists.txt + CMakePresets.json (schema v3, Ninja Multi-Config, binaryDir build/, CMAKE_EXPORT_COMPILE_COMMANDS ON). project(Duet VERSION 0.1.0 LANGUAGES C CXX).
- FetchContent, full-SHA pins, JUCE declared before Tracktion: JUCE f8f8864172464b9adf9eba6101e1f784838d1597, Tracktion 494e91d2ff546353b69723a5e992dd71d1a0204b with GIT_SUBMODULES "", Catch2 8b08d4d79514f45f7e4ce2a607ac9c94e920d1bb. TE_ADD_EXAMPLES OFF.
- modules/duet_model (Session facade), modules/duet_persistence (project layout), modules/duet_app (JUCE GUI shell), tests/ (Catch2, 3 tests).
- .clang-format and .clang-tidy at the repository root; AGENTS.md check commands made executable verbatim.

DECISIONS A REVIEWER NEEDS
1. One shared INTERFACE target, duet_engine_config, carries the engine link set and every JUCE/Tracktion definition. JUCE modules are INTERFACE sources — they compile into each consuming target — so two targets configured differently would be an ODR violation rather than a build error. Consequence, accepted: the engine compiles twice (duet_model and duet_app). duet_tests links duet_model's archive and does not recompile it.
2. Flags set: JUCE_JACK=1, TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH=1, JUCE_USE_CURL=0, JUCE_WEB_BROWSER=0, JUCE_PLUGINHOST_LADSPA=0 — exactly this issue's list. JUCE_PLUGINHOST_VST3 is deliberately NOT set here; it belongs to aty85a.
3. Two configure presets, linux-debug and linux-release, share the one Ninja Multi-Config build tree. That keeps `cmake --preset linux-debug` working verbatim as AGENTS.md documents it, and gives 3u1blw a Release path from a single configure. Both configure clean.
4. The engine-free rule holds by construction, not by discipline: each facade links duet_engine_config PRIVATE, so no engine or JUCE include directory reaches a consumer, and duet_tests links the two facades and nothing else. Provable from compile_commands.json — the test TU that includes both facade headers compiles with ZERO JUCE/Tracktion include paths, while duet_model's implementation TU has 11.
5. duet_model::Session is the minimal facade the skeleton needs: audioTrackCount / tempoBpm / editLengthSeconds, transport start/stop/isPlaying/position, audioDeviceDescription, plus loadDemoContent. Each Session owns a ScopedJuceInitialiser_GUI, a te::Engine and an in-memory te::Edit (Edit(Engine&, forEditing)) — that ownership is what makes the Catch2 test headless. performAction, undo/redo and the track/clip ops are quiwf3's, and were left alone.
6. loadDemoContent is explicitly temporary: an 8-second A-minor MIDI phrase through the engine's 4OSC, so the shell has something audible before quiwf3's vocabulary can build content. It is documented as such in the header and goes away with that slice.
7. duet_persistence holds only audioDirectory() — the folder shape ADR 0005 already fixes. The edit-file name, the DUET tree and the recovery file are 1c8sjh's and 3vwusn's decisions and were not pre-empted.
8. clang-tidy starts at bugprone/cert/clang-analyzer/concurrency/cppcoreguidelines/misc/modernize/performance/portability/readability with WarningsAsErrors '*'. Nine checks are off, each with its reason in the file — mostly checks that argue with a JUCE or Catch2 idiom (include-cleaner vs umbrella module headers, owning-memory vs setContentOwned, avoid-do-while and use-anonymous-namespace vs Catch2 macro expansions). Real findings were fixed, not suppressed: [[nodiscard]] on two accessors, a named parameter, std::array::at for the pitch lookup, defaulted destructors on the two JUCE classes.
9. clang-format is the JUCE house style (byte-for-byte the style Tracktion's own .clang-format sets) plus ColumnLimit 100.

CHECKS — all four green, run as AGENTS.md documents them
- Configure: cmake --preset linux-debug. Build: cmake --build --preset linux-debug -j 4.
- Format: clang-format-18 --dry-run --Werror over git ls-files — clean.
- Lint: clang-tidy-18 -p build/ over the Duet .cpp files — clean.
- Test: ctest --preset linux-debug --output-on-failure — 3/3 passed. "a new session opens one empty audio track at 120 bpm" creates and inspects a real in-memory engine Edit headlessly.

MANUAL AUDIO CHECK — PASSED (dev machine, 2026-08-18)
Launched with `pw-jack ./build/modules/duet_app/duet_app_artefacts/Debug/Duet`. The window opened, the audio device came up as JACK/PipeWire at 512 samples / 44.1 kHz, and the producer confirmed: Play produces the audible looping arpeggio through the engine transport, Stop halts it, and the position counter runs.

FACTS FOR THE NEXT SESSION
- clang-format-18 and clang-tidy-18 were not installed on the dev machine and were added during this session. Ubuntu ships only the version-suffixed binaries — there is no unversioned clang-format or clang-tidy — so AGENTS.md names the suffixed ones.
- AGENTS.md's Run line is now the concrete binary path, and Format/Lint name `git ls-files` as the Duet-sources selector, which can never reach the vendored trees under the ignored build/.
- The Debug build of everything from a cold FetchContent took roughly 6 minutes at -j 4 on the dev machine, in the band ddp1qt measured.
