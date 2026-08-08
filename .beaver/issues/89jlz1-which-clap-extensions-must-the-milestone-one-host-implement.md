---
id: 89jlz1
title: Which CLAP extensions must the milestone-one host implement?
state: todo
priority: high
labels:
    - roadmap:d9gioe
    - session:research
depends_on:
    - hvv3nn
parent: d9gioe
created: 2026-08-08T03:51:33Z
updated: 2026-08-08T03:51:33Z
---

Research session. Node hvv3nn settled *how* CLAP hosting is built — a `juce::AudioPluginFormat` + `juce::AudioPluginInstance` pair registered into Tracktion's `PluginManager::pluginFormatManager` — and established that no usable CLAP hosting library exists at any licence, so Duet writes this host itself. What it did not settle is *how much* host to write for milestone one.

Settle the extension set and the threading contract:

- Which CLAP extensions the milestone-one host must implement to load and run real-world CLAP instruments and effects correctly, and which can wait. Candidates: audio-ports, note-ports, params (including params-flush and the rescan/clear flags), state, gui, latency, tail, render, thread-check, timer-support, posix-fd-support, audio-ports-config, audio-ports-activation, note-name, voice-info.
- The threading contract Duet must honour. Every CLAP entry point is annotated `[main-thread]`, `[audio-thread]` or `[thread-safe]`; the host must respect those and should provide `clap_host_thread_check`. Establish what the host is obliged to guarantee, and where that collides with JUCE's message thread and Tracktion's audio thread.
- What the official MIT reference host (free-audio/clap-host) and clap-helpers actually implement — they are the closest thing to a specification of "enough".

Primary sources: the CLAP headers themselves (free-audio/clap `main`, `include/clap/ext/*.h` — the header comments are normative), free-audio/clap-host, free-audio/clap-helpers (its `clap::helpers::Host` template base class wraps `clap_host` with virtuals per extension and is the likely skeleton). Not recall, and not the dead jatinchowdhury18 module, which hvv3nn ruled out as a dependency.

Facts already in hand from hvv3nn, do not re-derive:

- The seam and what it gets for free (sidechain, state, automation, bus layout, editors, programs, latency, undo, Edit persistence) — read hvv3nn's closing note first.
- GUI: the embedding call sequence, the resize protocol, and the fact that `clap_host_posix_fd_support` + `clap_host_timer_support` are Linux's mandatory pair (CLAP's twin of VST3's `Linux::IRunLoop`, which JUCE already implements for VST3 and Duet must hand-write for CLAP).
- State: `clap_plugin_state::save/load` plus the partial-read/write loop `stream.h` mandates.
- Sidechain: CLAP has no sidechain concept; it is "input port index >= 1 lacking `CLAP_AUDIO_PORT_IS_MAIN`".

Deliverable: the milestone-one extension set with the reason for each inclusion and deferral, plus the threading obligations — sized so the next session can prototype against it, and ready to become part of the foundation spec.
