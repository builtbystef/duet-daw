---
id: ddp1qt
title: 'Walking skeleton: prove the chosen stack plays audio'
state: todo
priority: high
labels:
    - roadmap:d9gioe
    - session:prototype
depends_on:
    - lf8tnt
    - psmj4y
parent: d9gioe
created: 2026-08-07T06:02:50Z
updated: 2026-08-08T08:45:19Z
---

Prototype session (disposable code). Before the foundation spec is written, prove the chosen stack end to end: an app window from the chosen GUI foundation, audio device I/O, playback of a couple of mixed tracks, with acceptable latency, on the primary milestone-one platform.

The question it answers: does the chosen combination (foundation + engine layer + toolchain) actually hold together, and does anything about latency, build friction, or API shape force a revisit? Findings feed the foundation spec; the code is thrown away.

## Notes

**claude** — 2026-08-07T06:51:58Z

Load-bearing for the foundation decision (from node 1hn16k, 2026-08-07): this skeleton is where JUCE's chosen risk is proven or killed.

JUCE's Linux window peer offers ONLY the software renderer — getAvailableRenderingEngines() returns { "Software Renderer" } (modules/juce_gui_basics/native/juce_Windowing_linux.cpp). The framework-supported escape hatch is per-component: attaching an OpenGLContext routes a component's paint() through an OpenGLGraphicsContext. JUCE 9.0.0 also improved software-renderer performance and added OpenGL ES support on Linux.

So this prototype must render, on Linux, a DENSE SCROLLING TIMELINE — many clips, waveforms, a piano-roll-density grid — twice: once on the default software renderer, once with an attached OpenGLContext. Measure frame time while scrolling and zooming, not just at rest. Audio playback alone does not answer this question.

If neither path holds up, the foundation decision reopens, and the first alternative to evaluate is the Qt-GUI + JUCE-headless hybrid described in node 1hn16k's closing notes.

**claude** — 2026-08-08T01:30:04Z

Scope added by node lf8tnt (2026-08-07), which adopted Tracktion Engine as the engine layer. The skeleton now proves JUCE 9 + Tracktion Engine together, not JUCE alone. The Linux rendering mandate in the previous note is unchanged and still the load-bearing risk; these are additions to it.

BUILD-INTEGRATION QUESTIONS THIS SKELETON ANSWERS FIRST (cheap, and they gate everything after):
- Does Tracktion Engine 'develop' build cleanly against JUCE 9 in OUR build, not just in their CI? Their nightly juce_compatability workflow is green against JUCE develop (9.0.0), but their vendored submodule still pins a pre-9 JUCE (8.0.13) and no source declares JUCE 9 support. Decide and prove whether we use their vendored JUCE or supply our own.
- C++20 is required by Tracktion (above JUCE's C++17 floor).
- Enable the Signalsmith time-stretch backend and confirm it works. All four Tracktion stretch backends are compile-time-disabled by default, and with none enabled time-stretch is disabled outright. Signalsmith is bundled, MIT and header-only — the only backend with no external licence or link step (SoundTouch is LGPL-2.1, Rubber Band is GPLv2/commercial and incompatible with AGPLv3 JUCE without going commercial on both).

WHAT PLAYBACK NOW MEANS HERE: rather than hand-wiring an AudioProcessorGraph, load or build a Tracktion 'Edit' with a couple of tracks and play it — the engine rebuilds its audio graph from the model automatically (Edit::restartPlayback, driven by Edit::TreeWatcher on ValueTree changes). Worth confirming that the automatic rebuild is not audible as a glitch when the model changes during playback, since Duet's whole editing story — and the Collaborator's — depends on that being smooth.

STILL THE PRIMARY QUESTION: the dense scrolling timeline on Linux, under the software renderer and with an attached OpenGLContext, measured while scrolling and zooming. Note that JUCE's AudioThumbnail/AudioThumbnailCache is available for waveform drawing.

If a cheap round-trip test fits here, it would retire an open question in node rquzdc: write a custom property onto an engine-owned tree node (a TRACK or MIDICLIP), save, reload, and see whether it survives.

**claude** — 2026-08-08T08:45:19Z

Constraints and open risks from node psmj4y (2026-08-08), which settled the toolchain:

- THE SKELETON'S FIRST JOB IS NOW A BUILD QUESTION, NOT A RENDER ONE. It is unverified that Tracktion Engine develop (494e91d2ff546353b69723a5e992dd71d1a0204b, 2026-08-03) compiles against JUCE 9.0.0. Tracktion's root CMake guards JUCE acquisition with 'if(NOT TARGET juce::juce_core)', so supplying our own JUCE 9 first is POSSIBLE, and their nightly 'juce_compatability' workflow against JUCE develop was green — but the vendored submodule is still 8.0.13 and nothing declares JUCE 9 support. This is the single highest-risk assumption in psmj4y's answer. Fallback if it fails: use Tracktion's vendored JUCE 8.0.13 with a 'url.https://.insteadOf' rewrite for its SSH submodule URL, and revisit.
- THE BUILD RECIPE TO EXECUTE: CMake >= 3.22, project(... LANGUAGES C CXX) (JUCE hard-errors without a C compiler), Ninja Multi-Config via CMakePresets.json schema v3, CMAKE_EXPORT_COMPILE_COMMANDS ON, FetchContent with full-SHA pins, JUCE declared BEFORE Tracktion, TE_ADD_EXAMPLES OFF, and target_compile_features(... PRIVATE cxx_std_20) set BY HAND on every Duet target — nothing in the dependency graph enforces C++20 (Tracktion's modules declare no minimumCppStandard, so JUCE's machinery falls back to cxx_std_11).
- FLAGS TO SET: JUCE_JACK=1, JUCE_PLUGINHOST_VST3=1, TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH=1 (without a stretch flag, time-stretch is disabled entirely), JUCE_USE_CURL=0, JUCE_WEB_BROWSER=0, JUCE_PLUGINHOST_LADSPA=0. JUCE_ALSA already defaults to 1.
- NO VST3 SDK DOWNLOAD IS NEEDED TO HOST VST3. JUCE 9 bundles it at modules/juce_audio_processors_headless/format_types/VST3_SDK/ (relocated in JUCE 9). juce_set_vst3_sdk_path is for a custom SDK only; JUCE's own AudioPluginHost never calls it.
- APT PACKAGES TO INSTALL FIRST (not present on the dev machine as of 2026-08-08 — pkg-config --exists alsa and --exists jack both fail): libasound2-dev libjack-jackd2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libxi-dev libglu1-mesa-dev mesa-common-dev libegl-dev. The opengl three matter because 1hn16k found JUCE's Linux peer is software-only and an attached OpenGLContext is the escape hatch this node must prove.
- JACK IS A BUILD-TIME HEADER DEPENDENCY ONLY. JUCE dlopen/dlsym's libjack at runtime (juce_JackAudio.cpp holds a 'static void* juce_libjackHandle'), never links it. pipewire-jack is installed on the dev machine but NOT as the system-wide libjack — /etc/ld.so.conf.d/pipewire-jack-x86_64-linux-gnu.conf does not exist, only the example under /usr/share/doc. So launch the skeleton via 'pw-jack' until that is enabled.
- MEASURE THE BUILD TIME and report it. Tracktion's own cold, uncached Linux CI jobs run 4-11 min on a 4-core runner; psmj4y decided against a compiler cache on that evidence, and a materially worse number for Duet would reopen it.
- DEV MACHINE STATE (2026-08-08): Ubuntu 24.04.4, GCC 13.3.0, Clang 18.1.3, CMake 3.28.3, Ninja 1.11.1 — clears the full C++20 bar including <format>.
