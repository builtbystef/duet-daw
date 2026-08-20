---
id: 6i7an7
title: plugin.add's position counts the plugins a producer never put there
state: done
assignee: claude
priority: high
labels:
    - bug
parent: b1j3me
created: 2026-08-19T04:15:47Z
updated: 2026-08-20T08:05:52Z
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

## Notes

**claude** — 2026-08-20T08:05:52Z

Decision: a chain position counts the producer's plugins, and TrackInfo::plugins lists exactly that chain, so the vocabulary's position and the read-back index are one index space. Recorded as ADR 0007; ARCHITECTURE.md's duet_model bullet points at it.

The translation lives in `rawPositionFor` (SessionImpl.h) and both addPlugin and reorderPlugin go through it. The producer's plugins occupy one stretch of the raw chain — after the AuxReturn that feeds a bus, before the fader, the meter and any AuxSend — and a position counts inside that stretch and is clamped to its ends, so no position can put a plugin in front of the return that feeds it. `isProducersPlugin` is a blacklist of the four Duet puts there (VolumeAndPan, LevelMeter, AuxReturn, AuxSend), because an external VST3 is the producer's and is not enumerable.

Why the list drops Duet's plugins rather than marking them: the issue's "PluginInfo needs to say what a plugin is for" presumes they stay listed. With them unlisted the need goes away, and the stronger property holds — every entry is the producer's, and its index is the position to pass back in. Two index spaces in the same struct is the shape of the bug this issue is about. Nothing in the facade needs a ref to one of Duet's own: track volume and pan are on TrackInfo, the meter is Session::trackPeakDb, the return is TrackInfo::sends.

Tests: MixerOpsTests' "a send into a reverb bus is heard" now passes 0 rather than 1 — red before the change (tail 0.0), green after. PluginOpsTests gained "a chain position counts the producer's plugins, not the ones Duet put there": it asserts both chains read empty while the return, faders, meters and send exist, then adds two effects, reorders, and renders — the render is what proves reorder's position 0 is the same position 0, and it goes red (tail 0.0) if reorderPlugin skips the translation, which was checked.

The magic 1 is gone from Main.cpp and from DemoWalkthroughTests. Also recorded in ENGINE_NOTES.md, as a further fact proved with tests/scratch: a track the engine makes is born with a VolumeAndPanPlugin and a LevelMeterPlugin that stay at the end of the chain, and AuxReturn/AuxSend have no reserved place of their own.

All four checks pass; the full suite is 103/103.
