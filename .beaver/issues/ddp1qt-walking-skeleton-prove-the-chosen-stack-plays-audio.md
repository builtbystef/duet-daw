---
id: ddp1qt
title: 'Walking skeleton: prove the chosen stack plays audio'
state: done
assignee: claude
priority: high
labels:
    - roadmap:d9gioe
    - session:prototype
depends_on:
    - lf8tnt
    - psmj4y
parent: d9gioe
created: 2026-08-07T06:02:50Z
updated: 2026-08-09T20:25:44Z
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

**claude** — 2026-08-09T20:12:22Z

Interim findings from the live prototype session (2026-08-09), before closure:

BUILD GATE — PASSED. Tracktion Engine develop (494e91d2) compiles and links against supplied JUCE 9.0.0 (f8f8864) with ZERO source patches. psmj4y's highest-risk assumption is retired. Cold build ~2 min on the dev machine (well under the 4-11 min CI band). Two recipe corrections for psmj4y/l1gtax: (1) project() must declare VERSION or juce_add_gui_app hard-errors; (2) GCC needs -latomic linked explicitly — Tracktion's WaveNode uses a 16-byte std::atomic and nothing upstream links libatomic.

AUDIO GATE — PASSED. 5-track Edit (4 wave + 1 MIDI/4OSC) plays via ALSA/PipeWire, 512 smp @ 44.1kHz, 34.8 ms out-latency. 180 structural mutations applied mid-playback (clip insert/move/remove outside the audible loop): ZERO xruns, audio CPU <= 5.2%, and the producer could not hear the rebuilds. TreeWatcher graph rebuild is effectively inaudible.

API TRAP FOUND: AudioTrack::insertWaveClip stores the source path relative to the edit's TEMP directory ('../x.wav'), which silently resolves to nothing at playback — tracks play silence with no error anywhere. Fix: SourceFileReference::setToFile(file, PathStyle::alwaysAbsolute, false) after insert. This trap matters for every programmatic clip insertion Duet will ever do (including the Collaborator's).

OPEN: Save/Reload crash (app died ~4s after the test once; recurred after removing the second live Edit — now running under gdb to capture); render benchmark verdict pending (early numbers: SW 65-100fps, GL pinned ~52fps + one 3288ms attach stall).

**claude** — 2026-08-09T20:25:30Z

CLOSING NOTE — verdict and full record (2026-08-09). Prototype code: branch prototype/walking-skeleton, directory prototype-walking-skeleton/ (disposable; build with 'cmake --preset default && cmake --build --preset release', run via pw-jack).

VERDICT (user's words): "Yes, the stack is proven." JUCE 9 + Tracktion Engine develop + the psmj4y toolchain hold together end to end on Linux. Nothing about latency, build friction, or API shape forces a revisit. The foundation decision (1hn16k) and engine decision (lf8tnt) stand; the Qt-hybrid fallback is not needed.

RESULTS BY GATE
- Build: Tracktion develop (494e91d2) against supplied JUCE 9.0.0 (f8f8864) — compiles and links with zero source patches. Cold Release build ~2 min on the dev machine (Tracktion CI band is 4-11 min); psmj4y's no-compiler-cache decision unaffected. TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH=1 compiles (runtime stretch not exercised).
- Audio: 5-track Edit (4 wave + 1 MIDI through built-in 4OSC) plays on ALSA/PipeWire and as a JACK/PipeWire client (via pw-jack; 64-ch device). 512 smp @ 44.1 kHz, 34.8 ms reported output latency.
- Mutation-during-playback (the editing-story question): 600+ structural mutations (clip insert/move/remove outside the audible loop, so any artifact would be pure graph rebuild) — ZERO xruns, audio CPU <= 5.2%, producer heard nothing. TreeWatcher rebuild is effectively inaudible.
- Rendering (the 1hn16k load-bearing risk): 60-track/~3600-clip timeline with waveforms and piano-roll grids, measured while auto-scrolling and zoom-cycling with audio playing. Software renderer: 75-89 fps, worst frame 22.5 ms, up to 373 clips drawn. OpenGLContext on the viewport: vsync-paced 56-58 fps, worst 37.6 ms, no attach stall on retest; producer: "GL feels the same". BOTH paths pass; software is the simpler default, GL stays the escape hatch.
- rquzdc rider: custom property ('duetCustomProp') written on an engine-owned TRACK ValueTree node SURVIVED save/reload — proven twice: once through a full te::loadEditFromFile engine reload, once through flushState + file parse. Engine-owned trees carry Duet's custom properties.

BUGS HIT, AND HOW EACH WAS RESOLVED
1. CMake configure error 'Target duet_skeleton must have its VERSION argument set': project() had no VERSION. Fix: project(... VERSION x.y.z ...). Recipe correction for l1gtax.
2. Link failure 'undefined reference to __atomic_store': Tracktion's WaveNode uses a 16-byte std::atomic; GCC lowers it to libatomic calls and nothing upstream links it. Fix: target_link_libraries(... atomic). Recipe correction for l1gtax.
3. Wave tracks silent, no error anywhere: AudioTrack::insertWaveClip stored the source path relative to the edit's TEMP subdirectory ('../bass.wav'), which resolves to a nonexistent file at playback. Diagnosed by reading the saved edit XML. Fix: clip->getSourceFileReference().setToFile(file, SourceFileReference::PathStyle::alwaysAbsolute, false) after every programmatic insert. TRAP FOR DUET: every programmatic clip insertion (including every Collaborator Proposal that adds a clip) must pin the source reference deliberately.
4. SIGSEGV on save (app died on the Save/Reload button; reproduced 3x): gdb backtrace pinned it to tracktion::engine::EditSnapshot::refresh() — tracktion_EditSnapshot.cpp:227 calls pi->getLength() unconditionally after TWO null checks of pi. Any edit outside Tracktion's Project system (no ProjectItem) whose file already exists on disk crashes on EditFileOperations::save. UPSTREAM BUG, worth reporting to Tracktion. Workaround (and Duet's likely real path, since Duet does not adopt the Project system): edit->flushState() then write edit->state XML to the file directly — passes, app survives.
5. Prototype's own DSP bug (not stack-related): 'vibrato' generated as sin(2*pi*f(t)*t) chirps upward — frequency modulation without phase accumulation. Fix: integrate phase per sample. Kept here as a reminder that generated test audio needs the same care as product DSP.
6. Dead-end theory, corrected: the save crash was first attributed to the second live Edit created by te::loadEditFromFile (shared temp dir, background jobs). Removing it did NOT fix the crash — the real cause was bug 4. Recorded so the wrong lesson is not drawn later.

OBSERVATIONS THAT ARE NOT BUGS
- One 3288 ms frame stall on first OpenGL attach (run 2); did not reproduce on retest with the stall detector armed. Watch for it when GL is first attached under load.
- One ~1 s whole-window freeze seen by the producer during a software-renderer pass; the app's own paint deltas stayed at 17-20 ms throughout — presentation-side (XWayland/compositor, possibly gdb pausing), not the app. Consistent with the hvv3nn XWayland posture; keep an eye on it.
- pipewire-jack is not enabled system-wide on the dev machine; JACK mode needs the pw-jack wrapper (psmj4y already recorded this).

FEEDS: 86t5lu (foundation spec: renderer default, UI-thread findings, build recipe), skb4tp (programmatic edit layer: traps 3+4 land directly on it), rquzdc (custom-property rider answered), l1gtax (recipe corrections 1+2).
