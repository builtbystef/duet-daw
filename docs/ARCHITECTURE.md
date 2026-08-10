# Architecture

The modules of this system, and the seams between them. Update this file when the shape changes. Audits compare it with reality.

Duet DAW is a native desktop digital audio workstation, written in C++, built to make collaboration between a music producer and an AI easy.

No product code exists yet; the foundation spec (issue b1j3me) defines the modules below.

- **Audio engine** — playback, recording, and processing. Tracktion Engine on JUCE 9 (roadmap nodes 1hn16k, lf8tnt; proven end to end at ddp1qt). The engine owns the audio thread and rebuilds its graph from the project model automatically; no Duet code runs on the audio thread.
- **`duet_model`** — the edit vocabulary layer over the engine's `Edit`, and the engine seam: its public interface exposes zero engine or JUCE types (opaque uint64 refs, plain seconds/beats). Every project change — producer gesture or accepted Proposal — goes through `performAction`, the only undo-transaction boundary (ADR 0004). Proposal Audition (apply-and-revert, invisible to undo) lives here.
- **`duet_persistence`** — project folder lifecycle, snapshot save, autosave/recovery, and schema migration of the DUET tree (ADR 0005). Same engine-free facade rule.
- **`duet_app`** — the JUCE 9 GUI application shell: message loop, device management, transport, and the mount points the UI area fills. Software renderer by default; per-surface `OpenGLContext` as the escape hatch. The message thread is the sole writer of the project model; analysis, thumbnails, and offline renders run on worker threads.
- **`duet_gui`** — the producer-facing interface (spec issue 535bbo): arrangement timeline, piano roll, mixer, browser, transport bar, and the Collaborator panel's styling, in a single docked main window. Depends on `duet_model` (every edit gesture ends in `performAction`) and `duet_persistence`; hosted by `duet_app`. Every surface splits into a paintless view-model (geometry, grid/snap arithmetic, hit-testing, selection, view-state serialization — the test seam) and a thin `juce::Component` that only paints and forwards events. Per-project view state lives in a VIEW child of the DUET tree; app-global UI settings in the engine's PropertyStorage.
- **AI integration** — decided (spec issue js437t; ADRs 0002, 0003). Three parts, one seam:
  - The **Collaborator service**, a C++ module in the DAW: socket server, Task Run lifecycle, tool dispatch, the deterministic analysis layer with its per-track cache, the Proposal manager, and the estimate ledger.
  - The **sidecar**, a minimal Node host embedding pi's SDK, bundled as a standalone binary: agent loop, provider auth, model switching, streaming.
  - The **socket protocol** between them — newline-delimited JSON-RPC 2.0 over a local socket; the DAW is the server and spawns the sidecar. Every prompt, streamed event, cancellation, and tool call crosses this seam, and its contracts are transport-independent, so a native C++ loop can replace the sidecar without contract changes.
  Real-time rule: nothing on the AI side touches the audio thread — the socket has its own thread, project-model reads marshal onto the message thread, analysis renders on worker threads.
- **Build system** — settled (roadmap node psmj4y; details in spec b1j3me): CMake ≥ 3.22, Ninja Multi-Config presets, C++20 set by hand on every Duet target, FetchContent with full-SHA pins (JUCE 9 declared before Tracktion), Catch2 v3 tests, GitHub Actions CI on pinned ubuntu-24.04 with a `checks-pass` gate.
