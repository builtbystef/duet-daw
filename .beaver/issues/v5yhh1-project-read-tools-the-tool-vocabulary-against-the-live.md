---
id: v5yhh1
title: 'Project-read tools: the Tool Vocabulary against the live project'
state: todo
priority: medium
depends_on:
    - xy9438
    - 4r7nlj
parent: js437t
created: 2026-08-12T04:01:44Z
updated: 2026-08-12T04:01:44Z
---

## What to build

The five project-read tools of the Tool Vocabulary, dispatched from the sidecar's tool calls and answered from the live project model: the track list with its mixer state and routing, the arrangement with its sections and clip placement, MIDI note lists, automation lanes, and the plugin chain for engine built-in plugins. Provenance is structural: everything read from the project model crosses the seam as a bare scalar, because a bare value is by construction a fact. Buses are tracks — the master and every group are read through these same tools.

Reads execute on the message thread, the sole writer of the project model; the service thread marshals and waits. They read the authoritative state, never a second copy. The fod077 fixtures, rebuilt as real projects with their recorded defects fixed, become the regression corpus, driven through the protocol seam against the test-double sidecar.

## Acceptance criteria

- [ ] Each of the five tools answers a tool call with the fields the spec's contract names, and an unknown tool name yields an error result the run survives.
- [ ] Track list, worked: a MIDI track named "Bass" at −6.0 dB, pan 0, unmuted and unsoloed, one send to a bus at −12.0 dB, two clips, one built-in EQ, and volume automation → one entry with exactly those values, its output naming the bus track's id, and its automated parameters naming volume.
- [ ] The master bus and every group appear in the track list with their own kind, and each is accepted as a track id by every other tool that takes one.
- [ ] Arrangement, worked: a project at 128 BPM in 4/4 with an 8-bar "Intro" section and a clip starting at bar 5 for 4 bars, looped → exactly those numbers; a project that declares a key reports it as a bare value, and one that declares none omits the field entirely.
- [ ] MIDI, worked: a clip holding a note of pitch 60, start 1.0 beats, length 0.5 beats, velocity 100 returns exactly that note with a stable id; asking for a track without naming a clip returns every MIDI clip on that track.
- [ ] Automation, worked: a volume lane with points at 0.0 and 4.0 beats returns both, with their values, in time order; a plugin-parameter lane names its plugin and parameter.
- [ ] Plugin chain, worked: a track carrying a built-in EQ returns that plugin in chain order with its enabled state, its latency, and its parameters as bare scalars with names and units.
- [ ] A tool call naming a track, clip, or plugin id that does not exist returns an error the model can correct against — never a crash, never an empty success.
- [ ] Every tool result is produced by reading the authoritative project model on the message thread; no tool code runs on the audio thread and none of it takes a lock the audio callback can take.
- [ ] Prompt-cache discipline: the same project state serializes byte-identically twice, with stable content ahead of volatile content and no timestamps anywhere in a result.
- [ ] The fixture corpus is served end to end through the protocol seam, and every fixture's expected values are asserted from it.
