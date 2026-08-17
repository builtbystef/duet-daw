---
id: oocnng
title: 'The sidecar: pi embedded, the Collaborator system prompt, the bundled binary'
state: todo
priority: medium
depends_on:
    - d8k46e
    - xy9438
parent: js437t
created: 2026-08-12T04:03:20Z
updated: 2026-08-17T04:12:26Z
---

## What to build

The real other half of the seam, replacing the test double. A minimal Node host embedding pi's SDK — built-in tools empty, every tool custom and forwarded across the socket — that speaks the protocol the double has been standing in for: configuration, a run with streamed commentary and tool activity, cancellation mapped to the session's abort, a terminal event with its status, and shutdown.

The system prompt is Duet's own: the Collaborator's identity, the provenance rules, the instruction to hedge when confidence is low, and how a Suggestion should be shaped — never pi's coding-agent identity. Prompt-cache discipline holds: frozen content first, volatile deltas last, no timestamps or unordered serialization in a stable prefix. The host builds to a standalone binary with the bundler the prototype chose and ships inside the DAW install, invisible to the Target Producer.

## Acceptance criteria

- [ ] The DAW spawns the shipped binary, configuration round-trips, and a prompt against a configured provider streams commentary that reaches the DAW-side listener.
- [ ] Every tool the model calls arrives at the DAW as a tool call and its result returns to the model; the model's tool list holds the Tool Vocabulary and the write-tool and nothing else — no file, shell, or web tools.
- [ ] The system prompt in force is Duet's: a dump of the assembled prompt contains the Collaborator identity and the provenance rules and contains no coding-agent identity or instruction.
- [ ] Cancellation aborts the in-flight provider request: the run ends canceled and no commentary arrives after the terminal event.
- [ ] A provider error or an unauthenticated provider ends the run failed with a plain error string, and the sidecar stays alive and usable for the next run.
- [ ] Prompt-cache discipline, asserted on a dump: the stable prefix is byte-identical across two runs in the same project state, and nothing volatile appears before the volatile tail.
- [ ] The binary runs on a machine with no Node installed, and its size and cold-start latency are within the range the prototype recorded.
- [ ] The sidecar is invisible in normal use: no console window, no stray output on the DAW's streams, and its crash or absence never takes the DAW down.
- [ ] An end-to-end run against a real provider on a fixture project produces commentary grounded in tool results, recorded as a note with the tool trace.

## Notes

**claude** — 2026-08-17T04:12:26Z

Scope note (2026-08-17): drafting the Collaborator system prompt text is in-scope here, bound by js437t: Duet's own identity (never pi's coding-agent identity — including replacing pi-ai's silent 'helpful assistant' default recorded at d8k46e), the provenance rules, the low-confidence hedging instruction, Suggestion guidance, and prompt-cache discipline (frozen content first, volatile deltas last, no timestamps in stable prefixes).
