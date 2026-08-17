---
id: nfjr5x
title: 'Recording: record-arm, input selection, monitoring, and a take landing as one Action'
state: todo
priority: high
depends_on:
    - quiwf3
    - 1c8sjh
parent: b1j3me
created: 2026-08-17T04:09:52Z
updated: 2026-08-17T04:09:52Z
---

## What to build

The record path through the engine-free vocabulary, per spec b1j3me's thread model and kimula's milestone scope (single-take recording; comping, punch-in, and loop-recording stay milestone two). Ops: record-arm a track, select its input (an audio device channel or a MIDI input), and set input monitoring. Transport record start/stop drives the engine's record path, written with no UndoManager like the rest of transport. A finished take lands as exactly one Action ("Record Take"): the new clip appears with its source written under the project folder's audio/ subdirectory and pinned project-relative, so a single undo removes the take's clip (the audio file may remain on disk). Latency compensation is the engine's own — this slice verifies the recorded clip lands where the engine's compensation puts it, and reimplements nothing.

This slice owns the vocabulary and engine wiring only. The transport-bar UI (record button, countdown into an untitled project) is 1fumn6's; the header arm toggle is s1jzd4's — both drive the ops built here.

## Acceptance criteria

- [ ] Arm, input-selection, and monitoring ops exist on the facade; armed state and the chosen input read back; the facade's public headers stay free of engine and JUCE include paths.
- [ ] Record start/stop go through the facade with no UndoManager: undoing an earlier edit Action during recording neither stops the transport nor moves its position.
- [ ] A recorded MIDI take (driven headlessly through the engine's record path) produces a clip whose notes read back; the take lands as exactly one Action named "Record Take"; a single undo removes the clip digest-exactly and redo restores it.
- [ ] A recorded audio take writes its file under the project folder's audio/ subdirectory with a project-relative source reference and lands as one Action; undo removes the clip and does not delete the file. If the engine's audio-record path cannot be driven headlessly, this criterion moves to the manual check below and the automated coverage is the MIDI take.
- [ ] Recording into a brand-new untitled project works without any prior explicit save — the project is a real folder from the start (ADR 0005; 535bbo's lifecycle relies on this).
- [ ] Manual check, recorded as a note on this issue: a live take on the dev machine via pw-jack, with monitoring audible while recording (not an automated test).
