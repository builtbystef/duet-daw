---
id: d8k46e
title: 'Prototype: the sidecar as a standalone binary — footprint, startup, headless pi'
state: todo
priority: high
labels:
    - session:prototype
parent: js437t
created: 2026-08-12T04:00:53Z
updated: 2026-08-12T04:00:53Z
---

## What to build

The spec's one unmeasured number, settled before the sidecar is committed to. A disposable prototype that embeds pi's SDK in a headless Node host outside Electron — built-in tools empty, one custom tool is enough — bundles it to a standalone binary, and measures what that costs: disk footprint and cold-start latency to first token. It also settles the question left open at hcxgfv: whether the loop can be driven from the lighter core package alone, without the coding agent's built-in tools and system prompt.

The deliverable is a note with the numbers and a recommendation: keep the sidecar, or exercise the native-loop escape hatch that ADR 0003 preserves. The work stays on a prototype branch.

## Acceptance criteria

- [ ] A headless Node host embeds pi and runs one prompt with one custom tool, streaming text and tool calls to stdout — no Electron, no browser, no window.
- [ ] The built-in tools are provably absent: a dump of the assembled request shows the custom tool and nothing else.
- [ ] The host builds to a single standalone binary that runs on a machine with no Node installed.
- [ ] Recorded as a note: binary size, unpacked install footprint, and cold-start latency to first token as the median of five runs, with the bundler named.
- [ ] Recorded as a note: whether the core package alone can drive the loop without the coding agent's tools and prompt, and what that changes in footprint.
- [ ] The note ends with a recommendation — keep the sidecar or take the native-loop escape hatch — justified by those numbers.
- [ ] The work stays on a prototype branch; nothing merges to main.
- [ ] Closure waits for user review.
