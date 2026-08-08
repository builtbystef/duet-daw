# Architecture

The modules of this system, and the seams between them. Update this file when the shape changes. Audits compare it with reality.

Duet DAW is a native desktop digital audio workstation, written in C++, built to make collaboration between a music producer and an AI easy.

No code exists yet. The decided shape so far:

- **Audio engine** — playback, recording, and processing. Tracktion Engine on JUCE 9 is adopted (roadmap nodes 1hn16k, lf8tnt); the foundation spec records the details and ADRs when it lands.
- **GUI** — the desktop interface, on JUCE 9.
- **AI integration** — decided (spec issue js437t; ADRs 0002, 0003). Three parts, one seam:
  - The **Collaborator service**, a C++ module in the DAW: socket server, Task Run lifecycle, tool dispatch, the deterministic analysis layer with its per-track cache, the Proposal manager, and the estimate ledger.
  - The **sidecar**, a minimal Node host embedding pi's SDK, bundled as a standalone binary: agent loop, provider auth, model switching, streaming.
  - The **socket protocol** between them — newline-delimited JSON-RPC 2.0 over a local socket; the DAW is the server and spawns the sidecar. Every prompt, streamed event, cancellation, and tool call crosses this seam, and its contracts are transport-independent, so a native C++ loop can replace the sidecar without contract changes.
  Real-time rule: nothing on the AI side touches the audio thread — the socket has its own thread, project-model reads marshal onto the message thread, analysis renders on worker threads.
- **Build system** — undetermined (roadmap node psmj4y).
