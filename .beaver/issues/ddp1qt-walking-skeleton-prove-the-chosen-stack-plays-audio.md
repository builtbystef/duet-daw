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
updated: 2026-08-07T06:51:58Z
---

Prototype session (disposable code). Before the foundation spec is written, prove the chosen stack end to end: an app window from the chosen GUI foundation, audio device I/O, playback of a couple of mixed tracks, with acceptable latency, on the primary milestone-one platform.

The question it answers: does the chosen combination (foundation + engine layer + toolchain) actually hold together, and does anything about latency, build friction, or API shape force a revisit? Findings feed the foundation spec; the code is thrown away.

## Notes

**claude** — 2026-08-07T06:51:58Z

Load-bearing for the foundation decision (from node 1hn16k, 2026-08-07): this skeleton is where JUCE's chosen risk is proven or killed.

JUCE's Linux window peer offers ONLY the software renderer — getAvailableRenderingEngines() returns { "Software Renderer" } (modules/juce_gui_basics/native/juce_Windowing_linux.cpp). The framework-supported escape hatch is per-component: attaching an OpenGLContext routes a component's paint() through an OpenGLGraphicsContext. JUCE 9.0.0 also improved software-renderer performance and added OpenGL ES support on Linux.

So this prototype must render, on Linux, a DENSE SCROLLING TIMELINE — many clips, waveforms, a piano-roll-density grid — twice: once on the default software renderer, once with an attached OpenGLContext. Measure frame time while scrolling and zooming, not just at rest. Audio playback alone does not answer this question.

If neither path holds up, the foundation decision reopens, and the first alternative to evaluate is the Qt-GUI + JUCE-headless hybrid described in node 1hn16k's closing notes.
