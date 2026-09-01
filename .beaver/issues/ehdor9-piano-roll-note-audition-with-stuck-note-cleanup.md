---
id: ehdor9
title: Piano Roll note audition with stuck-note cleanup
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
parent: kmsbfq
created: 2026-09-01T18:37:38Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Add a direct, engine-free note-audition seam to the current MIDI track and use it for piano keys and note-edit gestures. It bypasses recording inputs and never changes project state.

## Settled behavior

- `Session::startNoteAudition(track,pitch,velocity)` injects live MIDI into that track's output graph; `stopNoteAudition` and `stopAllNoteAuditions` send matching note-offs. Invalid/non-MIDI/no-instrument tracks do nothing.
- Audition velocity is 80. A piano-key press sustains until release; dragging over keys switches monophonically. Moving/adding a note sounds its current pending pitch; a pitch change sends off then on. A double-click add pulses for 150 ms through a commandable timer.
- Audition coexists with playback and hardware MIDI because it injects after input routing. It is not presented to recording devices and cannot enter a take.
- Release, Escape, focus loss, clip/project replacement, plugin/track deletion, transport stop, undo/redo, and component destruction stop every note owned by the audition seam.

## Acceptance and tests

- [ ] A fake note sink observes exact on/off order for key press/drag, edit pitch change, pulse, and every cleanup path without synthesized UI timing.
- [ ] A feature test hears the current instrument at the requested pitch and then silence; no Action, dirty state, or recorded note appears.
- [ ] Component tests cover keyboard hit routing and focus loss; model tests cover lifecycle and invalid tracks.

Start in `Session`, `PianoRoll`, `PianoRollCanvas`, and a small public note-sink/controller seam. Run all AGENTS.md checks before closing.
