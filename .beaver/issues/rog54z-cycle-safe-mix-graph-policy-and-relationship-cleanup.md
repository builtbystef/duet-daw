---
id: rog54z
title: Cycle-safe mix-graph policy and relationship cleanup
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
parent: jsfhhg
created: 2026-09-01T18:36:20Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Make output, send, and sidechain edits enforce one model-level directed-graph policy before the UI exposes them. Extend the public facade only with engine-free capability/choice facts.

## Settled graph rules

- Directed edges are track output `track -> group`, send `track -> group`, and sidechain `source -> plugin owner track`. Main Output/None are no edge.
- A destination must exist; output/send destinations must be Group tracks; a sidechain target must report sidechain capability and its source may be Audio, MIDI, or Group. Master is never a source/destination in these operations.
- Reject self-edges and any edge for which the destination already reaches the source through any mix-graph edge. Refusal creates no state/Action and returns a producer-facing reason through the existing notice seam.
- Add `removeSend(track, group)` as a real operation; `-60 dB` remains a quiet existing send rather than deletion. Remove the hidden aux return when its last send disappears.
- Deleting a track/group/plugin clears every now-dangling output, send, and sidechain relationship in the same deleting Action. Undo restores the complete graph.
- `PluginInfo` states `canSidechain`; no component probes engine buses.

## Acceptance and tests

- [ ] Worked acyclic combinations succeed and direct/indirect cycles across mixed edge kinds are rejected.
- [ ] Remove and deletion cleanup leave no hidden return/send, broken ref, or extra undo step; undo is digest-exact.
- [ ] Plugins without a sidechain are distinguishable from capable plugins with None selected.
- [ ] Save/reopen preserves the graph and capability reads remain stable.
- [ ] Tests drive `Session`/`EditOps` public seams in `MixerOpsTests.cpp` and `TrackOpsTests.cpp`.

Run targeted red/green tests, then all AGENTS.md checks before closing.
