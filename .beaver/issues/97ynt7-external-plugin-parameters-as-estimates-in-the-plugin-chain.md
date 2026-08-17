---
id: 97ynt7
title: External plugin parameters as estimates in the plugin chain
state: todo
priority: medium
depends_on:
    - 2z0y5u
    - aty85a
parent: js437t
created: 2026-08-12T04:02:25Z
updated: 2026-08-17T04:10:28Z
---

## What to build

A hosted VST3's parameters cross the seam differently from a built-in's. Duet owns its built-ins' semantics, so their parameters are bare scalars in real units. An external plugin's meaning belongs to its vendor, so its parameters carry the vendor's own name, a normalized value in 0..1, and the plugin's own UI display text wrapped as an estimate — the display string is what the plugin says, and what it means is a guess. That wrapped string feeds the same estimate ledger, so a run that inspected an external plugin's parameters marks its output exactly as a key estimate would.

Each plugin in the chain also reports its format, so the model can tell the two kinds apart without inferring it.

## Acceptance criteria

- [ ] Worked: a track carrying a built-in compressor and a hosted VST3 → the built-in's parameters are bare scalars in real units with names and units; the VST3's parameters carry vendor name, a normalized value in 0..1, and a wrapped display string; each plugin reports its format and its latency.
- [ ] The wrapped display string is the plugin's own text, unaltered, and its method says exactly that.
- [ ] Reading an external plugin's parameters taints the run's estimate ledger; reading only built-ins leaves the ledger empty.
- [ ] Plugins appear in chain order, and a disabled plugin reports itself disabled while still listing its parameters.
- [ ] A plugin that is missing or fails to load appears in the chain with its name and is reported as unavailable, never omitted silently.
- [ ] No plugin scan and no plugin load happens as a side effect of a tool call: only plugins already in the project's chains are reported.
- [ ] A parameter read that fails on an already-hosted plugin (an error or exception surfaced through the hosting layer) returns an error result the run survives: the run continues, and the DAW keeps working. Crash isolation is explicitly not asserted — hosting is in-process (b1j3me, hvv3nn; only scanning is out of process), so a plugin that brings the process down brings the DAW down; surviving that arrives only with milestone-two out-of-process hosting.
