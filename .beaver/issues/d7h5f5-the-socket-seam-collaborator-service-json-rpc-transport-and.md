---
id: d7h5f5
title: 'The socket seam: Collaborator service, JSON-RPC transport, and the sidecar process'
state: todo
priority: high
depends_on:
    - sea14w
parent: js437t
created: 2026-08-12T04:01:14Z
updated: 2026-08-12T04:01:14Z
---

## What to build

The DAW half of the AI seam (ADR 0003). A Collaborator service module that listens on a local socket, speaks newline-delimited JSON-RPC 2.0, and owns the sidecar as a child process: spawned lazily on first Collaborator use with the socket path as its argument, killed on DAW exit, respawned on demand when found dead. `configure` and `shutdown` round-trip across it.

The socket is serviced by the service's own thread — nothing here runs on the message thread or the audio thread. Everything is verified against a test-double sidecar that speaks the protocol and nothing else: no Node, no LLM, no UI. That double is the harness every later protocol slice reuses, and it is also the escape-hatch guarantee — anything that speaks this protocol can replace the sidecar.

## Acceptance criteria

- [ ] With the service started, a test-double sidecar connects to the socket and `configure` returns a success result.
- [ ] Framing, worked: two complete requests delivered in a single write, each newline-terminated, are handled as two requests; a request carrying an escaped newline inside a string value is handled as one.
- [ ] A request split across several reads is handled once its terminating newline arrives, and not before.
- [ ] A malformed line yields a JSON-RPC parse error response and the connection survives; an unknown method yields a method-not-found error and the connection survives.
- [ ] The sidecar is spawned on first Collaborator use, not at DAW start: a session that never invokes the Collaborator spawns no child process.
- [ ] A sidecar killed externally is detected, and the next use spawns a fresh one whose `configure` round-trips again.
- [ ] DAW exit terminates the sidecar and removes the socket; no orphan process and no stale socket file survive.
- [ ] `shutdown` ends the sidecar cleanly, and a second `shutdown` against an already-dead sidecar is harmless.
- [ ] A second connection attempt while a sidecar is connected is refused, and the connected sidecar is unaffected.
- [ ] All socket, framing, and process-management code runs on the service's own thread; none of it runs on the message thread or the audio thread, and none of it takes a lock the audio callback can take.
