---
id: 6i7an7
title: plugin.add's position counts the plugins a producer never put there
state: todo
priority: high
labels:
    - bug
parent: b1j3me
created: 2026-08-19T04:15:47Z
updated: 2026-08-19T04:15:47Z
---

## What is wrong

`EditOps::addPlugin (track, builtin, position)` passes `position` straight to the engine's `PluginList::insertPlugin`, so it is a raw index into the track's whole plugin chain — which already holds plugins Duet put there for its own reasons and the producer never asked for: the volume-and-pan fader every track is born with, the `AuxReturnPlugin` `setSend` inserts at index 0 of a bus, and the `AuxSendPlugin` it appends to the source track.

So "add a reverb at position 0" on a send bus puts the reverb *in front of the return that feeds the bus*. The reverb then sits upstream of the only thing that could feed it, processes silence, and is never heard. It happened in the vocabulary demo (4r7nlj) and the reverb was inaudible; the demo now passes 1, a magic number that is only correct because `setSend` happens to insert the return at 0.

This matters beyond the demo because js437t's `suggest` vocabulary has `plugin.add` with a position, and a Suggestion that says "put a reverb first in the chain" means first among the producer's effects. Taken literally against the raw index it produces a plugin that does nothing, silently — no error, no read-back that looks wrong, just no sound.

## What to decide and build

Whether `position` counts the producer's chain rather than the engine's, and the same question for `reorderPlugin` and for what `TrackInfo::plugins` lists. Options as they look now:

- Make the position a producer-chain index: `addPlugin` and `reorderPlugin` translate to and from the raw index, and `TrackInfo::plugins` either stops listing the infrastructure plugins or marks them so a caller can tell them apart.
- Keep raw indices and make the infrastructure visible, so a caller can compute a correct position — worse for a Suggestion, which would have to understand Duet's internals to place a plugin.

Either way `PluginInfo` needs to say what a plugin is for, since today a fader and an aux return are indistinguishable from a producer's plugin except by name.

## Acceptance criteria

- [ ] The decision is recorded, in an ADR if it changes the facade's meaning.
- [ ] `addPlugin (bus, reverb, 0)` on a bus that has a send into it produces a reverb that is heard.
- [ ] `reorderPlugin` uses the same index space as `addPlugin`, and a test says so.
- [ ] `TrackInfo::plugins` lets a caller tell a producer's plugin from one Duet put there.
- [ ] The demo's magic 1 in `Main.cpp` is gone.

## Already covered

MixerOpsTests has "a send into a reverb bus is heard, and rings on after the source stops", which renders a stab through a send into a reverb bus and asserts the tail outlives the source. It fails against the reverb placed at 0. That test should keep passing whatever this issue decides.
