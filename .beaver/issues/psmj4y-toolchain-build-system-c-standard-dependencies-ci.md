---
id: psmj4y
title: 'Toolchain: build system, C++ standard, dependencies, CI'
state: done
assignee: claude
priority: medium
labels:
    - roadmap:d9gioe
    - session:research
depends_on:
    - 1hn16k
parent: d9gioe
created: 2026-08-07T06:02:31Z
updated: 2026-08-08T08:44:05Z
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

**claude** — 2026-08-08T08:44:05Z

Research session closed (2026-08-08). All claims from primary sources; four parallel sub-agents (CMake integration, dependency acquisition, compiler/platform matrix, CI shape). One claim was verified empirically by downloading and building against the artifact.

## Question

Given JUCE 9 (node 1hn16k) and Tracktion Engine (node lf8tnt): what build system, C++ standard, dependency-management mechanism, compiler/platform matrix, and minimal CI shape does Duet adopt? Deliverable: the concrete toolchain choices, ready to become an ADR and to close `l1gtax` when implemented.

## Answer

**CMake ≥ 3.22 with the Ninja Multi-Config generator, driven by a checked-in `CMakePresets.json`; C++20 set explicitly on Duet's own targets; every dependency acquired by `FetchContent` pinned to a full commit SHA; Linux x86_64 with a GCC 13 floor; GitHub Actions on a pinned `ubuntu-24.04` runner with no compiler cache.**

Four findings drove this, and each of them overturns a default that looked obvious going in:

1. **Nothing in the dependency graph enforces C++20.** Tracktion's three module headers declare no `minimumCppStandard` at all, so JUCE's module machinery falls back to `cxx_std_11` for them; JUCE 9's own modules declare `17`. The C++20 requirement lives only in Tracktion's README and in each example's hand-written `target_compile_features(... cxx_std_20)`. Duet must set it itself or it will silently build at a lower standard.

2. **Tracktion's CMake accepts an externally-supplied JUCE** — its root guards acquisition with `if(NOT TARGET juce::juce_core)`. Making JUCE 9 available *first* skips the vendored submodule entirely. That matters more than it sounds: Tracktion's `.gitmodules` uses an **SSH** URL (`git@github.com:…`), which breaks anonymous recursive clones in CI. Supplying our own JUCE dissolves that problem instead of working around it, and is also the only way to get JUCE 9 rather than the pinned 8.0.13.

3. **ONNX Runtime's shipped CMake config package is broken in the Linux tarball**, verified by building against it. `find_package(onnxruntime CONFIG)` fails at configure time because the generated config points at `lib64/` and `include/onnxruntime/` while the tarball ships `lib/` and flat headers. Duet declares its own IMPORTED target instead. vcpkg and Conan are both several minor versions behind and, more decisively, have no Tracktion Engine or RTNeural port at all.

4. **A cold, uncached, full JUCE + Tracktion Linux build-and-test takes 4–11 minutes on a free 4-core runner** — measured from Tracktion's own green CI run of 2026-08-08. Upstream runs 31 such jobs nightly with zero caching. Actions is free and unlimited on public repos. So the CI question answers itself: build from scratch every push, add a compiler cache only if that number ever stops being true.

### The choices, concretely

| Axis | Choice |
|---|---|
| Build system | CMake, `cmake_minimum_required(VERSION 3.22)`, `LANGUAGES C CXX` |
| Generator | Ninja Multi-Config (`ninja-multi`), via `CMakePresets.json` schema v3 |
| C++ standard | C++20, `target_compile_features(duet_* PRIVATE cxx_std_20)` on every Duet target |
| Dependencies | `FetchContent`, every `GIT_TAG` a full commit SHA; JUCE declared before Tracktion |
| ONNX Runtime | Prebuilt release tarball via `FetchContent` URL + `URL_HASH`, hand-written IMPORTED target, behind one option |
| Tests | Catch2 v3 for Duet's own code; JUCE `UnitTestRunner` only if engine-level tests are ever needed |
| Platform (milestone one) | Linux x86_64 only |
| Compiler floor | GCC 13 / libstdc++ 13 primary; Clang 18 secondary (on libstdc++) |
| CI | GitHub Actions, `ubuntu-24.04` pinned, Debug + Release, actions pinned to SHAs |
| Sanitizers | ASan+UBSan and TSan+UBSan as two separate nightly configs — never combined |

## Findings

### Build system and the CMake floor

- **JUCE 9.0.0 requires CMake 3.22**: "Version 3.22 or higher is required." — JUCE README, tag 9.0.0. Its root declares `cmake_minimum_required(VERSION 3.22)` and `project(JUCE VERSION 9.0.0 LANGUAGES C CXX)`.
- **JUCE hard-errors without a C compiler**: "A C compiler is required to build targets that depend on JUCE. Add 'C' to your project's LANGUAGES." — JUCE 9.0.0 root `CMakeLists.txt` L34-42. Duet's `project()` must list `C CXX`.
- **Tracktion declares `cmake_minimum_required (VERSION 3.15...3.20)`** — lower than JUCE's, so JUCE's floor governs. — tracktion_engine `develop` root `CMakeLists.txt`.
- **JUCE 9's app-facing CMake API** all exists at 9.0.0: `juce_add_gui_app`, `juce_add_console_app`, `juce_add_plugin`, `juce_generate_juce_header`, `juce_add_binary_data`, `juce_add_pip`, `juce_disable_default_flags`, and the `juce_set_<kind>_sdk_path` family. JUCE also generates and installs `JUCEConfig.cmake`, so `find_package(JUCE)` is supported. — `extras/Build/CMake/JUCEUtils.cmake`, JUCE 9.0.0 root `CMakeLists.txt` L55-168.
- **JUCE root options a consumer sets**: `JUCE_MODULES_ONLY` OFF, `JUCE_BUILD_EXTRAS` OFF, `JUCE_BUILD_EXAMPLES` OFF, `JUCE_ENABLE_MODULE_SOURCE_GROUPS`. — same file.
- **Presets schema version → CMake version**: v1/3.19 (`configurePresets`), v2/3.20 (`buildPresets` + `testPresets`), **v3/3.21** (`condition`), v4/3.23, v6/3.25 (workflow presets), v8/3.28 (`$schema`). — cmake.org `cmake-presets(7)`. Tracktion itself pins `"version": 3` with `cmakeMinimumRequired: 3.22.0`, using `condition` + `${hostSystemName}` to gate its Xcode/Windows presets — exactly the shape a Linux-first-with-later-ports project wants. Duet matches: schema v3.
- "CMakePresets.json may be checked into a version control system, and CMakeUserPresets.json should NOT be checked in." — same docs. `.gitignore` gets `CMakeUserPresets.json`.
- **`CMAKE_EXPORT_COMPILE_COMMANDS` "is implemented only by Makefile Generators and Ninja Generators"** and was added in CMake 3.5. — cmake.org. Choosing Ninja is therefore also what makes clang-tidy and editor tooling possible.

### C++ standard — the load-bearing gap

- **Tracktion's module headers declare no `minimumCppStandard`.** The `BEGIN_JUCE_MODULE_DECLARATION` blocks of `tracktion_engine.h`, `tracktion_core.h` and `tracktion_graph.h` (all `version: 3.5.0` on develop) carry only ID/vendor/version/name/description/website/license/dependencies. — the three module headers, `develop`.
- **The fallback is C++11.** `JUCEModuleSupport.cmake` L583-589: `_juce_get_metadata(... minimumCppStandard module_cpp_standard)` → `if(module_cpp_standard) target_compile_features(${module_name} INTERFACE cxx_std_${module_cpp_standard}) else() target_compile_features(${module_name} INTERFACE cxx_std_11) endif()`. — JUCE 9.0.0.
- **JUCE 9's own modules declare `minimumCppStandard: 17`** — `juce_core`, `juce_events`, `juce_graphics`, `juce_gui_basics`, `juce_gui_extra`, `juce_audio_devices`, `juce_audio_processors`, `juce_audio_processors_headless`, `juce_audio_utils`, `juce_audio_formats` all at tag 9.0.0. JUCE's README states the same: "__C++ Standard__: 17 (20 when building with JUCE_USE_WINDOWS_MIDI_SERVICES enabled)".
- **So the transitively enforced floor for a Tracktion app is C++17, not C++20.** No `CMAKE_CXX_STANDARD` is set anywhere in Tracktion's CMake. C++20 appears only per-target in the examples: `target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)` in DemoRunner (L59), Benchmarks (L41), TestRunner (L49), EngineInPluginDemo (L56).
- **Contradiction, recorded as such**: Tracktion's README says "*N.B. Tracktion Engine requires C++20*", while its own build machinery declares nothing above C++11 for its modules. Both are primary. The README is the intent; the CMake is the enforcement, and it is absent. Duet resolves this by setting `cxx_std_20` explicitly on its own targets — which is also what every Tracktion example does.
- **JUCE upstream tests C++20 as a first-class configuration**: its private CI dispatcher exposes an input `cpp-std` — "The C++ standard to use (optional [20, 23])". — `.github/workflows/juce_private_build.yml`, master.

### Dependency acquisition

**Chosen: `FetchContent`, full-SHA pins, JUCE declared first.**

- CMake's own documentation gives the pinning rule: "Where contents are being fetched from a remote location and you do not control that server, it is advisable to use a hash for `GIT_TAG` rather than a branch or tag name," because "A commit hash is more secure and helps to confirm that the downloaded contents are what you expected." — cmake.org `FetchContent`. `FetchContent` added 3.11; `FetchContent_MakeAvailable` 3.14 — both well below our 3.22 floor.
- **Tracktion accepts an external JUCE.** Its root `CMakeLists.txt` does `if(NOT TARGET juce::juce_core)` → `CPMAddPackage("gh:juce-framework/JUCE#develop")` when `JUCE_CPM_DEVELOP` is set, else `add_subdirectory(modules/juce)`. A parent that makes JUCE available first skips both branches. It similarly guards `if(NOT TARGET tracktion_core)` and otherwise only adds the `tracktion::` aliases. — `develop`.
- **What a consumer links**: `modules/CMakeLists.txt` calls `juce_add_modules(... ALIAS_NAMESPACE tracktion tracktion_core tracktion_engine tracktion_graph)`. Targets are `tracktion::tracktion_core`, `tracktion::tracktion_engine`, `tracktion::tracktion_graph`, linked alongside JUCE modules directly (`juce::juce_audio_devices`, `juce::juce_audio_processors`, `juce::juce_audio_utils`, `juce::juce_recommended_warning_flags`).
- **There is no `find_package` path for Tracktion.** No `*Config.cmake`/`.in` anywhere, `cmake/` holds only a vendored `CPM.cmake`, and no `install(EXPORT …)` exists. `INSTALL_PATH` in `juce_add_module` only copies module sources — it does not export targets. Source inclusion is the only mechanism. Every Tracktion example demonstrates it: `add_subdirectory(../../modules/juce …)` then `add_subdirectory(../../modules …)`.
- **Pins as of this session**:
  - JUCE — tag `9.0.0` (released 2026-07-21), pinned by its commit SHA. It is the newest JUCE release.
  - Tracktion Engine — `develop` HEAD `494e91d2ff546353b69723a5e992dd71d1a0204b`, authored 2026-08-03T20:52:40Z. (`master` is stale; latest tag `v3.2.0` predates ~1.5 minor versions of develop work — established at lf8tnt and unchanged.)
  - RTNeural — `main` `31da15bb957942a1515f355370d90a2f1d975a5a`, 2026-07-22. **RTNeural has no git tags at all**, so a commit pin is the only option that exists.
- **Tracktion options to set**: exactly one CMake `option()` exists — `TE_ADD_EXAMPLES`, **defaulting ON**, which also calls `enable_testing()` and pulls in Benchmarks, TestRunner, DemoRunner and EngineInPluginDemo. Duet sets `TE_ADD_EXAMPLES OFF`. There is no separate tests/benchmarks option; those *are* the examples.
- **Tracktion pulls no external dependency but JUCE.** Everything else is vendored: `modules/3rd_party/` holds choc, crill, doctest, expected, libsamplerate, magic_enum, nanorange, rigtorp, rpmalloc; `modules/tracktion_engine/3rd_party/` holds airwindows, signalsmith-stretch, soundtouch.

**Rejected mechanisms, with the primary-source reason:**

- **vcpkg** — `ports/tracktion*` and `ports/rtneural` do not exist (404), and `ports/juce/vcpkg.json` is `"version": "8.0.7"`, i.e. JUCE 8. Its `onnxruntime` port is `1.23.2` (five minors behind). vcpkg also has no lockfile; pinning is via `builtin-baseline` + `overrides`, and Microsoft warns that a manifest without `builtin-baseline` "operates according to the Classic mode algorithm and **ignores all versioning information**." Two of five dependencies are simply absent, which settles it.
- **Conan** — no `juce`, `rtneural` or `tracktion-engine` recipe; `onnxruntime` is at `1.24.4`. Same failure, same reason.
- **git submodules** — Tracktion's `.gitmodules` points `modules/juce` at `git@github.com:juce-framework/JUCE.git` (SSH), branch `develop`. An SSH submodule URL breaks anonymous recursive clones without a `url.https://.insteadOf` rewrite. RTNeural likewise needs `--recursive` for its `xsimd` submodule. Supplying JUCE ourselves and using FetchContent avoids both.
- **CPM.cmake** (MIT, v0.43.1) — by its own README "a thin wrapper around CMake's FetchContent module that adds version control, caching, a simple API and more," with `CPM_SOURCE_CACHE` for offline configure and `CPMUsePackageLock()` for a lockfile. Genuinely useful, and Tracktion already vendors it — but it adds a layer for benefits (offline cache, lockfile) that a five-dependency project with explicit SHA pins does not need. Not excluded forever; simply not warranted now.

### ONNX Runtime and RTNeural

- **ONNX Runtime v1.28.0, published 2026-07-25**, licence **MIT** ("Copyright (c) Microsoft Corporation"). — GitHub releases API; repo LICENSE.
- **Linux x64 CPU prebuilt: `onnxruntime-linux-x64-1.28.0.tgz`, 9,125,960 bytes (8.7 MB) compressed, ~25 MB unpacked**, containing `lib/libonnxruntime.so.1.28.0` (24.3 MB) + `libonnxruntime_providers_shared.so`, flat `include/`, `lib/cmake/onnxruntime/*.cmake`, `lib/pkgconfig/libonnxruntime.pc`. Also published: `linux-aarch64`, `win-x64` (+arm64), `osx-arm64`. The CUDA variants are 240–424 MB and irrelevant here.
- **The shipped CMake config package is broken in that tarball — verified empirically.** `onnxruntimeTargets.cmake:62` sets `INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/include/onnxruntime"` (tarball has flat `include/`), and `onnxruntimeTargets-release.cmake` sets `IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib64/libonnxruntime.so.1.28.0"` (tarball ships `lib/`). A minimal `find_package(onnxruntime CONFIG REQUIRED)` under CMake 3.28.3 fails at configure: *"The imported target \"onnxruntime::onnxruntime\" references the file …/lib64/libonnxruntime.so.1.28.0 but this file does not exist."* `libonnxruntime.pc` carries the same wrong assumptions. Microsoft's own install doc never mentions `find_package` for the C/C++ tarball — it says only "Extract it. Move and include the header files in the `include` directory."
  - **Duet declares its own `add_library(onnxruntime SHARED IMPORTED)`** against `lib/` and `include/`. Symlinking `lib64 -> lib` and `include/onnxruntime -> .` also works, but a hand-written IMPORTED target is fewer moving parts and does not depend on an upstream bug staying the same shape.
- **Building ONNX Runtime from source is not practical via FetchContent**: the repo has **no root `CMakeLists.txt`** (the CMake root is `cmake/`), its documented build is a wrapper script (`./build.sh --config RelWithDebInfo --build_shared_lib …`), prerequisites are "Install Python 3.10+" and "Install cmake-3.28 or higher", it needs `git clone --recursive`, and `cmake/external/onnxruntime_external_deps.cmake` pulls protobuf, abseil and onnx at configure time. Prebuilt is the only sane path.
- **RTNeural: BSD 3-Clause.** Not header-only — `add_library(RTNeural STATIC …)` with `POSITION_INDEPENDENT_CODE ON`; consumed by `add_subdirectory` + `target_link_libraries(... RTNeural)` (plain target, no namespace alias). Backend options `RTNEURAL_EIGEN`/`RTNEURAL_XSIMD`/`RTNEURAL_STL` all default OFF, and the `else()` branch selects **Eigen as the default**.
- **Backend vendoring is mixed, and it decides the backend for us**: `modules/Eigen/` and `modules/json/` are checked directly into the repo, but `modules/xsimd` is a **git submodule**. A FetchContent/tarball fetch therefore gets Eigen but not xsimd. Duet takes the default Eigen backend. Eigen is MPL-2.0 — file-level weak copyleft, compatible with a closed commercial edition, and not LGPL. RTNeural states the obligation itself: "you must abide by the licensing rules of whichever backend library you choose."
- **Separability, as u24m3x required**: the whole ONNX Runtime + RTNeural + Basic Pitch path sits behind one CMake option (`DUET_ENABLE_POLYPHONIC_TRANSCRIPTION`, default ON). Nothing else in the build may reference it. The user has already accepted dropping polyphonic transcription if the runtime causes build trouble; this makes dropping it a `-D` flag rather than surgery.

### Tests

- **Catch2 v3.15.3 (2026-07-26), Boost Software License 1.0** — chosen. Consumed by `FetchContent_Declare(Catch2 GIT_REPOSITORY … GIT_TAG <sha>)`, link `Catch2::Catch2WithMain`, then `include(CTest)` + `include(Catch)` + `catch_discover_tests(tests)`. Gotcha to remember: with FetchContent you must `list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)` before `include(Catch)`.
- GoogleTest v1.17.0 (2025-04-30), BSD-3-Clause, is equally viable. Catch2 wins on two small margins: BSL-1.0 imposes no binary-form attribution at all (marginally simpler for a closed edition), and it is the more actively released of the two. Neither has any AGPL or commercial-edition conflict; this is a low-stakes choice and reversible.
- **Both JUCE and Tracktion ship their own test mechanism.** JUCE has the `juce::UnitTest` base class (`modules/juce_core/unit_tests/juce_UnitTest.h`, guarded by `JUCE_UNIT_TESTS`) plus a ready runner at `extras/UnitTestRunner/`. Tracktion's `examples/TestRunner/` builds on it, collecting `UnitTest::getTestsInCategory("tracktion_core" | "tracktion_graph" | "tracktion_engine" | "Tracktion")` and emitting **JUnit XML** for CI; Tracktion's own tests are `juce::UnitTest` subclasses inside the module sources.
- **Consequence**: Catch2 covers Duet's own code. If engine-level tests that exercise Tracktion's own test bodies are ever wanted, they must run through the JUCE `UnitTestRunner` path — a second, separate test target, not a Catch2 concern.

### Platform and compiler matrix

- **JUCE 9.0.0 minimum build requirements** (README, tag 9.0.0): "__Linux__: g++ 7.0 or Clang 6.0"; "__Windows__: Visual Studio 2019 (Windows 10 version 1607)"; "__macOS/iOS__: Xcode 12.4 (Intel macOS 10.15.4, Apple Silicon macOS 11.0)".
- **JUCE 9.0.0 deployment targets**: "__Linux__: Mainstream Linux distributions (x86_64, Arm64/aarch64 …)"; "__macOS__: macOS 10.11 (x86_64, Arm64)"; "__Windows__: Windows 10 version 1607 (x86_64, x86, Arm64, Arm64EC)".
- **JUCE's floor is irrelevant — C++20 sets the real one.** From GCC's and libstdc++'s own status pages: concepts GCC 10 (202002 at 10.2), coroutines default-on at GCC 11, `<ranges>` libstdc++ 10.1, `std::jthread`/`stop_token` 10.1, three-way comparison library 10.1 — but **`std::format` (P0645R10) only at libstdc++ 13.1**, three releases after the rest. With libc++: `<ranges>` complete at LLVM 15, and `__cpp_lib_format` not set until LLVM 19 despite the implementation being complete since LLVM 14.
- **Therefore: GCC 13 is the floor**, and Clang is supported on libstdc++ 13+ rather than libc++. Ruling out `<format>` would drop the floor to GCC 11; that is not a trade worth making for a project starting in 2026.
- **The developer machine already clears it** (local observation, 2026-08-08): Ubuntu 24.04.4 LTS x86_64, kernel 7.0.0-28-generic, GCC/G++ 13.3.0, Clang 18.1.3, CMake 3.28.3, Ninja 1.11.1, pkg-config 1.8.1. A compiled `<version>` probe under `g++-13 -std=c++20` reports `__cpp_lib_format=202110`, `__cpp_lib_ranges=202110`, `__cpp_lib_jthread=201911`, `__cpp_lib_three_way_comparison=201907`, `__cpp_impl_coroutine=201902`, `__cpp_concepts=202002`. Clang 18 on libstdc++ matches except `__cpp_concepts=201907`.
- **Milestone one is Linux x86_64 only.** Windows and macOS are a matrix line added later, not a port to design now. One fact to carry forward: **ONNX Runtime 1.28.0 ships no `osx-x64` asset** — Apple Silicon only. An Intel Mac build would need a source build or an older release.

### Linux system packages

JUCE 9.0.0's `docs/Linux Dependencies.md` is the authority (its own scope note: "tested on Ubuntu 16.04 LTS …, 18.04 LTS …, and 20.04 LTS", with a separate note that the freetype/fontconfig packages named "are available on Ubuntu 22 and 24"). Per module, with the ones Duet disables struck out:

- `juce_audio_devices` → `libasound2-dev`, `libjack-jackd2-dev`
- `juce_graphics` → `libfontconfig1-dev`, `libfreetype-dev`
- `juce_gui_basics` → `libx11-dev`, `libxcomposite-dev`, `libxcursor-dev`, `libxext-dev`, `libxinerama-dev`, `libxrandr-dev`, `libxrender-dev`, `libxi-dev`
- `juce_opengl` → `libglu1-mesa-dev`, `mesa-common-dev`, `libegl-dev`
- `juce_core` → `libcurl4-openssl-dev` — **dropped**, `JUCE_USE_CURL=0`
- `juce_gui_extra` → `libwebkit2gtk-4.1-dev` — **dropped**, `JUCE_WEB_BROWSER=0`
- `juce_audio_processors` → `ladspa-sdk` — **dropped**, `JUCE_PLUGINHOST_LADSPA=0` (milestone one is VST3-only, node u24m3x)

`juce_opengl` is on the list because node 1hn16k found JUCE's Linux peer offers only the software renderer, making an attached `OpenGLContext` the escape hatch that ddp1qt must prove.

Only three JUCE 9 modules declare `linuxPackages` at all — `juce_graphics` (`freetype2 fontconfig`), `juce_audio_devices` (`alsa`), `juce_opengl` (`egl gl`); `juce_core` declares `linuxLibs: rt dl pthread`. The X11 packages are compile-time header requirements documented in the doc, not pkg-config link entries.

**Not yet installed on the dev machine** (local `apt-cache policy`, 2026-08-08): `libasound2-dev`, `libjack-jackd2-dev`, `libpipewire-0.3-dev`. `pkg-config --exists alsa` and `--exists jack` both fail. Installed: `pipewire` 1.0.5, `pipewire-jack` 1.0.5, with `pipewire`/`pipewire-pulse`/`wireplumber` all active.

### Audio backend flags

- **`JUCE_ALSA` defaults to 1; `JUCE_JACK` defaults to 0.** Duet sets `JUCE_JACK=1`. — `juce_audio_devices.h` L165-177, JUCE 9.0.0.
- **JUCE needs only the JACK *headers* at build time.** `juce_audio_devices.cpp` L235 `#include <jack/jack.h>` under `JUCE_JACK`, with the in-source comment "Linux: The package you need to install to get JACK support is libjack-dev." At runtime `juce_JackAudio.cpp` holds `static void* juce_libjackHandle` and resolves every symbol by `dlsym` — libjack is never linked. So `libjack-jackd2-dev` is a build-only dependency and the runtime `.so` can come from anywhere.
- **That is exactly what makes the PipeWire path work.** PipeWire's own man page for `pw-jack`: it "modifies the LD_LIBRARY_PATH environment variable so that applications will load PipeWire's reimplementation of the JACK client libraries instead of JACK's own libraries", and is unnecessary if "PipeWire's reimplementation of the JACK client libraries has been installed as a system-wide replacement". Ubuntu's `pipewire-jack` installs `libjack.so.0` under `/usr/lib/x86_64-linux-gnu/pipewire-0.3/jack/` plus `/usr/bin/pw-jack`, and ships the activating `ld.so.conf.d` file only as an **example** under `/usr/share/doc/pipewire/examples/`. On this machine that file is not enabled, so Duet must be launched via `pw-jack` until it is — a developer-environment fact worth recording for ddp1qt.
- Ubuntu `libjack-jackd2-dev` (1.9.21~dfsg-3ubuntu3, noble) additionally `Provides: libjack-dev` and `Conflicts: libjack-dev`.

### Plugin hosting — a premise retired

- **No VST3 SDK download is needed to host VST3 in JUCE 9.** JUCE 9.0.0 bundles the SDK at `modules/juce_audio_processors_headless/format_types/VST3_SDK/` (relocated in JUCE 9 — the old `juce_audio_processors/format_types/` path 404s at that tag). `juce_VST3Headers.h` compiles the SDK sources directly, and the only `JUCE_CUSTOM_VST3_SDK` requirement is on BSD. The contrast is explicit in the option docs: `JUCE_PLUGINHOST_VST` says "You will need to have the VST2 SDK files in your header search paths"; `JUCE_PLUGINHOST_VST3` says no such thing. JUCE's own `extras/AudioPluginHost` sets `JUCE_PLUGINHOST_VST3=1` and never calls `juce_set_vst3_sdk_path`.
- **All `JUCE_PLUGINHOST_*` options default to 0** and now live in `juce_audio_processors_headless.h` (JUCE 9 split the plugin-format code out of `juce_audio_processors`, which now declares `dependencies: juce_gui_extra, juce_audio_processors_headless`). Duet sets `JUCE_PLUGINHOST_VST3=1` and leaves the rest at 0.

### Tracktion compile definitions to set deliberately

These are **compile definitions, not CMake options** — they are set in `target_compile_definitions`, and all default to 0 in `tracktion_engine.h` (develop, L152-211):

- **`TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH=1`** — Duet's choice. "This is a header-only library, so no additional linking is required," and it is vendored at `modules/tracktion_engine/3rd_party/signalsmith-stretch` with an in-tree `LICENSE.txt`. With no stretch flag set, time-stretch is disabled entirely. The alternatives are all foreclosed by lf8tnt's licence findings anyway — Elastique ("You must have Elastique in your search path"), RubberBand ("not owned by Tracktion and is licensed separately"), SoundTouch (LGPL-2.1). Note Tracktion's own DemoRunner picks SoundTouch; Duet does not.
- Defaults left alone: `TRACKTION_ENABLE_SINGLETONS` 0, `TRACKTION_ENABLE_ARA` 0, `TRACKTION_ENABLE_CMAJOR` 0, `TRACKTION_ENABLE_ABLETON_LINK` 0, `TRACKTION_ENABLE_FFMPEG` 0, `TRACKTION_ENABLE_LIBLAME` 0, `TRACKTION_ENABLE_CONTROL_SURFACES` 0, `TRACKTION_AIR_WINDOWS` 0, `TRACKTION_UNIT_TESTS` 0, `TRACKTION_BENCHMARKS` 0. Two default **on** and are worth knowing about: `TRACKTION_ENABLE_PLUGIN_CPU_MEASUREMENT` 1 and `TRACKTION_LOG_DEVICES` 1.
- The practical JUCE definition template, from Tracktion's DemoRunner: `JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_STRICT_REFCOUNTEDPOINTER=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0` — Duet takes the last three and decides `JUCE_MODAL_LOOPS_PERMITTED` when the UI is designed.

### CI

**Cost is not a constraint.** "GitHub Actions usage is free for self-hosted runners and for public repositories that use standard GitHub-hosted runners," and "Use of the standard GitHub-hosted runners is free and unlimited on public repositories." The real limits: 6 h per job, 35 days per workflow run, 20 concurrent jobs on the Free plan, 256 jobs per matrix, artifacts/logs retained 90 days by default (settable 1–90 for public repos). Standard Linux x64 runner: **4 CPU, 16 GB RAM, 14 GB storage**. Larger runners are Team/Enterprise only.

**Runner images** (actions/runner-images, main, 2026-08-08):
- `ubuntu-latest` currently resolves to `ubuntu-24.04`, but "The `-latest` migration process is gradual and happens over 1-2 months" — Duet pins `ubuntu-24.04` explicitly.
- `ubuntu-24.04` (image 20260720.247.2): Ubuntu 24.04.4 LTS; **GCC 12.4.0, 13.3.0, 14.2.0**; Clang 16.0.6, 17.0.6, 18.1.3; CMake 3.31.6; Ninja 1.13.2. No ALSA/JACK/X11 *dev* packages preinstalled → CI installs the JUCE list itself.
- `ubuntu-22.04` (image 20260720.234.2): GCC 10.5/11.4/12.3 only — **no GCC 13, therefore no `<format>` in libstdc++. Ruled out.**
- For later ports: `windows-latest` → `windows-2025`; `macos-latest` → `macos-26` (arm64), with `macos-26-intel` for x64.

**How long the build actually takes.** Measured from Tracktion's own green run `31233601201` (2026-08-08, `develop`), cold and uncached on a 4-core `ubuntu-latest`, build **and** test: DemoRunner 4.0/5.2 min (Debug/Release), EngineInPluginDemo 4.1/5.1, TestRunner 10.6/8.7, Benchmarks 29.3/6.8. The slowest job across the whole 31-job matrix was macOS **TSan** Benchmarks at **146.2 min**.

**Upstream caches nothing.** Tracktion's `build.yaml` contains no `actions/cache`, no ccache, no sccache, no `*_COMPILER_LAUNCHER`, and no `timeout-minutes` on any job — the whole JUCE+Tracktion tree is recompiled every run, 31 jobs a night. Its build commands are presets: `cmake --preset <p>` then `cmake --build --preset <p>-<config> --target <t>`, Linux preset `ninja-multi`, tests by `ctest --test-dir $TEST_DIR -C $CONFIG -R $TARGET -V`.

**Duet's decision: no compiler cache on day one.** If one is ever needed, sccache is the only candidate with first-party CI guidance — its README states "sccache is also available as a GitHub Actions to facilitate the deployment using GitHub Actions cache," it caches C/C++, and it plugs in via `-DCMAKE_CXX_COMPILER_LAUNCHER=sccache` ("cmake 3.4 or newer"); the action is maintainer-owned (`mozilla-actions/sccache-action`, last push 2026-08-03) and backs onto the Actions cache directly. **ccache's own manual never mentions CI or GitHub at all.** GitHub's own CMake starter workflow uses no cache either. Cache facts if it ever matters: 10 GB per repository, entries evicted after 7 days unaccessed, and "Workflow runs cannot restore caches created for child branches or sibling branches" — so a cache must be warmed on the default branch.

**The shape:**

1. `format` — `clang-format --dry-run --Werror` over Duet's own sources. No build, seconds. `-n`/`--dry-run` = "do not actually make the formatting changes"; `--Werror` = "changes formatting warnings to errors". `.clang-format` is found in "the closest parent directory of the input file"; base styles available are LLVM, Google, Chromium, Mozilla, WebKit, Microsoft, GNU.
2. `build-and-test` on `ubuntu-24.04`, Debug + Release via Ninja Multi-Config, `ctest --output-on-failure`. `--test-dir <dir>` was added in CMake 3.20, `--output-junit` in 3.21. `enable_testing()` is required, and "The `CTest` module invokes `enable_testing` automatically unless `BUILD_TESTING` is set to `OFF`."
3. `checks-pass` gate — a single job with `if: always()` and `needs: [...]` that fails unless the matrix succeeded, used as the one required status check. Both Tracktion workflows end in exactly this, and it means branch protection never needs updating when the matrix changes. This is the one structural idea worth copying wholesale.
4. **Nightly `schedule`**: ASan+UBSan, and TSan+UBSan as a *separate* configuration.
5. `clang-tidy` — driven from a `compile_commands.json` over **an explicit list of Duet's own sources**, not `CMAKE_CXX_CLANG_TIDY`. That variable (added CMake 3.6) runs clang-tidy on every TU CMake compiles, which for a project vendoring two large trees means linting all of JUCE's and Tracktion's amalgamated module `.cpp` files — an explosion of CI time and third-party diagnostics. clang-tidy discovers `.clang-tidy` "in the closest parent directory of the source file" and takes `--warnings-as-errors`.
6. **Pin every action to a full-length commit SHA.** GitHub: "Pinning an action to a full-length commit SHA is currently the only way to use an action as an immutable release," because a tag "can be moved or deleted if a bad actor gains access to the repository storing the action." Neither JUCE nor Tracktion does this (they pin `actions/checkout@v6`, and JUCE pins one third-party action to a *branch*) — Duet does, since it costs nothing.
7. **No CLA workflow, no fork-PR hardening, no `pull_request_target`.** Node a3p83b closed outside contributions, which deletes an entire layer of CI concern. (JUCE's public CI is essentially only a CLA check plus a dispatcher into a private repo.)

**Sanitizers are forced into two configurations by the toolchain, not by preference.** GCC states it directly: for TSan, "The option cannot be combined with -fsanitize=address, -fsanitize=leak"; for ASan, "The option cannot be combined with -fsanitize=thread or -fsanitize=hwaddress." Clang enforces the same at the driver level (`SanitizerArgs.cpp` `IncompatibleGroups` pairs `Address` with `Thread | Memory`). **`Undefined` appears in no incompatible pair**, so ASan+UBSan and TSan+UBSan are both legal — which is exactly how Tracktion's own `CMakePresets.json` is built: `ninja-multi-asan` sets `-fsanitize=address,undefined -fsanitize-address-use-after-scope`, `ninja-multi-tsan` sets `-fsanitize=thread`, never merged.

Costs, from the tools' own docs: ASan "typical slowdown … is 2x" and "exits on the first detected error. This is by design"; TSan "slowdown … about 5x-15x", memory "about 5x-10x"; UBSan's "checks have small runtime cost and no impact on address space layout or ABI" but **recovers and continues by default** — CI must pass `-fno-sanitize-recover=...` to get an exit code. TSan belongs on `schedule`, not `push`: Tracktion gates it out of push runs explicitly (`SKIP_STEPS=1` when `matrix.tsan && github.event_name == 'push'`), and their TSan job took 146 minutes.

### For AGENTS.md and issue `l1gtax`

The commands to record once the stack lands:

- **Configure** — `cmake --preset linux-debug` (Ninja Multi-Config, `CMAKE_EXPORT_COMPILE_COMMANDS=ON`)
- **Build** — `cmake --build --preset linux-debug`
- **Test** — `ctest --preset linux-debug --output-on-failure`
- **Format** — `clang-format -i` over Duet sources; check with `clang-format --dry-run --Werror`
- **Lint** — `clang-tidy -p build/ <duet sources>`
- **Typecheck** — n/a; the compiler is the typechecker

## Unresolved

- **Whether Tracktion Engine `develop` (494e91d) actually compiles against JUCE 9.0.0.** The `if(NOT TARGET juce::juce_core)` guard makes supplying JUCE 9 *possible*, and Tracktion's nightly `juce_compatability` workflow builds against JUCE `develop` and was green on 2026-08-07 — but the vendored submodule remains pre-9 (37c894f, 8.0.13), no README or CMake statement declares JUCE 9 support, and no build was attempted here. **This is the single highest-risk assumption in this node's answer, and it is `ddp1qt`'s to settle** — the walking skeleton either compiles or it does not, and no amount of further reading substitutes for that. Fallback if it does not: use Tracktion's vendored JUCE 8.0.13 via a `url.https://.insteadOf` rewrite for the SSH submodule, and revisit.
- **Tracktion Engine's licence is self-contradictory in its own primary sources.** `LICENSE.md` is dual **GPL3-or-later / Commercial**, while the three module headers declare `license: Proprietary`. Which governs is not determinable from the repo. Duet ships AGPLv3 (a3p83b); GPLv3 §13 explicitly permits combining GPLv3 code with AGPLv3 code, so the combination appears sound — but this is a legal reading, not a research finding, and it is not this node's decision to make. Flagged for the pre-commercial checklist already recorded at a3p83b, which requires contacting Tracktion sales regardless.
- **The licences of nine of Tracktion's twelve vendored third-party directories.** Only choc, crill and signalsmith-stretch carry a top-level LICENSE file; the rest would require reading per-file headers. Tracktion's own `LICENSE.md` disclaims responsibility: "it may also contain some 3rd-party code from other open-source projects. It is your responsibility when using this codebase to ensure you comply with the terms of all code within it." Not chased, because none of the nine is enabled by Duet's option set. Revisit only if one is turned on.
- **Whether the ONNX Runtime `lib64`/`include/onnxruntime` config mismatch also affects the Windows `.zip` and macOS `.tgz` assets.** Only the Linux x64 tarball was verified empirically. The generation logic in `cmake/CMakeLists.txt` is platform-independent, so the same mismatch is likely wherever `CMAKE_INSTALL_LIBDIR` resolves to `lib64`, but this was not tested. Moot for milestone one; it is a Windows/macOS-port item.
- **How long a full Duet build actually takes in CI.** The 4–11 min figures are for Tracktion's own targets built via its presets. A DAW app linking more JUCE modules could differ. Measurable only by building it — another `ddp1qt` observation.
- **How much sccache would save on this stack.** Neither sccache's nor GitHub's docs quantify hit rates for a JUCE-sized tree. Irrelevant unless the build time above turns out badly.
- **Whether `pipewire-jack` can be made the system-wide libjack on Ubuntu by an officially supported switch** beyond copying the shipped example `ld.so.conf.d` file. No first-party Ubuntu or PipeWire statement on the intended activation procedure was found. A developer-environment inconvenience, not a product question — Duet runs under `pw-jack` meanwhile.
- **Whether `--test-dir` and `--preset` can be combined before CMake 3.30.** The ctest docs attach an "Added in version 3.30" note to that combination specifically and do not state the 3.20–3.29 behaviour. Our floor is 3.22; if the combination misbehaves, invoke ctest directly with `--test-dir`, which is what Tracktion does (it defines no `testPresets` at all).
- **JUCE's real CI shape** — runners, build commands, caching, timeouts. `juce-framework/JUCE-utils` is private (404); only the dispatcher and its `cpp-std: [20, 23]` input are observable. Tracktion's CI stood in as the evidence.

## Sources

JUCE 9.0.0: README.md; root CMakeLists.txt; docs/Linux Dependencies.md; docs/CMake API.md; extras/Build/CMake/JUCEUtils.cmake; extras/Build/CMake/JUCEModuleSupport.cmake; extras/AudioPluginHost/CMakeLists.txt; modules/{juce_core,juce_events,juce_graphics,juce_gui_basics,juce_gui_extra,juce_audio_devices,juce_audio_processors,juce_audio_processors_headless,juce_audio_utils,juce_audio_formats,juce_opengl}/*.h; juce_audio_devices.cpp; native/juce_JackAudio.cpp; juce_audio_processors_headless/format_types/juce_VST3Headers.h; juce_core/unit_tests/juce_UnitTest.h; .github/workflows/juce_private_build.yml; releases API.
Tracktion Engine (develop @ 494e91d): CMakeLists.txt; modules/CMakeLists.txt; CMakePresets.json; LICENSE.md; .gitmodules; cmake/CPM.cmake; modules/tracktion_{engine,core,graph}/*.h; examples/{DemoRunner,Benchmarks,TestRunner,EngineInPluginDemo}/CMakeLists.txt; examples/TestRunner/TestRunner.h; tests/utils.cmake; .github/workflows/build.yaml; contents + commits + actions APIs (run 31233601201).
CMake: FetchContent; cmake-presets(7); ctest(1); add_test; CMAKE_EXPORT_COMPILE_COMMANDS; CMAKE_<LANG>_CLANG_TIDY.
ONNX Runtime: repo LICENSE; cmake/CMakeLists.txt @ v1.28.0; onnxruntime.ai/docs/install; onnxruntime.ai/docs/build/inferencing; releases API; the unpacked onnxruntime-linux-x64-1.28.0.tgz (empirical build test, CMake 3.28.3).
RTNeural: LICENSE; CMakeLists.txt; cmake/ChooseBackend.cmake; README; .gitmodules; tags API. Eigen: gitlab.com/libeigen/eigen COPYING.README.
vcpkg: ports/{juce,onnxruntime,catch2,gtest}/vcpkg.json; learn.microsoft.com vcpkg versioning. Conan: conan-center-index recipes/onnxruntime/config.yml. CPM.cmake: LICENSE, README.
Catch2: docs/cmake-integration.md, LICENSE, releases. GoogleTest: quickstart-cmake, LICENSE, releases.
GCC: cxx-status.html; libstdc++ manual status.html; Instrumentation-Options.html. Clang/LLVM: cxx_status.html; libcxx Status/Cxx20.html; ClangFormat.html; ClangFormatStyleOptions.html; clang-tidy/index.html; AddressSanitizer.html; ThreadSanitizer.html; UndefinedBehaviorSanitizer.html; clang/lib/Driver/SanitizerArgs.cpp.
GitHub: docs.github.com Actions billing, limits, github-hosted-runners, dependency-caching-reference, artifact retention, security/secure-use; actions/runner-images README + Ubuntu2404-Readme.md + Ubuntu2204-Readme.md; actions/starter-workflows ci/cmake-single-platform.yml; mozilla/sccache README; Mozilla-Actions/sccache-action README; ccache/ccache doc/manual.adoc.
PipeWire: docs.pipewire.org page_man_pw-jack_1. Ubuntu: packages.ubuntu.com/noble/libjack-jackd2-dev.
Local machine (Ubuntu 24.04.4, 2026-08-08): lsb_release, uname, gcc/g++/clang/cmake/ninja/pkg-config --version, apt-cache policy/show, dpkg -L, pkg-config --exists, systemctl --user is-active, a compiled <version> feature-macro probe.
