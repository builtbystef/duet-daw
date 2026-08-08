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
updated: 2026-08-08T01:30:04Z
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
