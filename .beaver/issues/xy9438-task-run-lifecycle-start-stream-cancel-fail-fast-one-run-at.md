---
id: xy9438
title: 'Task Run lifecycle: start, stream, cancel, fail fast, one run at a time'
state: todo
priority: medium
depends_on:
    - d7h5f5
parent: js437t
created: 2026-08-12T04:01:28Z
updated: 2026-08-12T04:01:28Z
---

## What to build

A Task Run across the seam: `run.start` carrying the prompt and the opening context, the streamed notifications coming back — commentary deltas, tool activity, and the terminal event with its status — and `run.cancel`. The rules around them are the point: one active run at a time, with the panel's input closed while it runs; cancel tears down and reports "canceled"; a provider error, an unreachable backend, or a dead sidecar fails the run fast; nothing queues, nothing retries, and the DAW is never blocked waiting.

This slice also lands the DAW-side listener interface a UI attaches to later. Exercised entirely against the test-double sidecar — no LLM, no Node, no UI.

## Acceptance criteria

- [ ] `run.start` carries the prompt and the opening context — selection kind and ids, playhead bar and beat, whether the transport is playing — and returns immediately; completion arrives later as a terminal event.
- [ ] Streamed commentary deltas reach a registered listener in order, and their concatenation equals what the double sent.
- [ ] Tool-activity start and end notifications reach the listener with their tool names, in the order sent.
- [ ] A second `run.start` while a run is active is rejected and the active run is unaffected.
- [ ] `run.cancel` ends the active run with status canceled; the listener sees exactly one terminal event, and a further cancel of the same run is harmless.
- [ ] Worked: the double reports the run finished with status failed and an error string → the listener sees one failed terminal event carrying that string, and the service accepts a new run immediately afterwards.
- [ ] Sidecar death mid-run fails that run with a terminal event; nothing is queued and nothing is retried, and the next run spawns a fresh sidecar.
- [ ] Events naming an unknown or already-finished run id are ignored, and no terminal event is ever delivered twice for one run.
- [ ] The DAW never blocks on a run: starting, cancelling, and receiving events all return without waiting on the sidecar, asserted against a double that delays its responses.
- [ ] A canceled or failed run leaves no partial state behind: the next run starts as cleanly as the first.
