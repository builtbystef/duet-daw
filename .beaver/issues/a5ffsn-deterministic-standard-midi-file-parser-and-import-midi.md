---
id: a5ffsn
title: Deterministic Standard MIDI File parser and Import MIDI Action
state: done
assignee: agent
priority: high
labels:
    - session:task
    - roadmap:yfpnps
parent: kmb4mv
created: 2026-09-01T18:34:33Z
updated: 2026-09-01T21:34:23Z
---

## Bounded implementation

Add an engine-free MIDI import result and one model operation that materializes it as an ordinary MIDI clip/notes. No chooser, desktop drop, or worker UI belongs here.

## Settled MIDI policy

- Accept SMF format 0 and 1. Merge note events from all source tracks and channels into one clip; channel is discarded because milestone-one `NoteInfo` has none, and coincident notes remain distinct.
- For PPQ files, one quarter note is one project beat. For SMPTE files, convert source seconds to beats using the project's tempo at import; imported tempo/metre/key events never alter the project.
- Times remain relative to file time zero, preserving leading silence. Pair overlapping same-pitch notes FIFO within source track/channel; ignore unmatched note-offs and reject unmatched note-ons with a reported count rather than inventing lengths.
- Sort materialized notes by start, then pitch, then source track/channel/event order. Clip length is the latest valid note end, with a minimum of the current grid subdivision.
- Unsupported non-note events are counted in an import summary, not materialized. A file with no valid notes is an error and emits no Action.
- One `Import MIDI` Action inserts the clip at the supplied snapped beat and all notes; undo removes it digest-exactly and no source `.mid` path persists.

## Acceptance and tests

- [ ] Literal format-0, format-1, PPQ, and SMPTE fixtures produce independently stated beat/pitch/length/velocity values.
- [ ] Tempo/metre events do not change the project; malformed/empty files produce an actionable result and no Action.
- [ ] Merge order and summary text are deterministic across runs.
- [ ] Tests drive the parser and public model Action seam; no JUCE component or wall-clock timing is involved.

Place parsing in an engine-free model/helper API, with tests in a focused `tests/MidiImportTests.cpp`. Run all AGENTS.md checks before closing.

## Notes

**agent** — 2026-09-01T20:39:03Z

Seams: parseStandardMidiFile (engine-free bytes → MidiImport) and EditOps::importMidi inside performAction("Import MIDI"). Tests in tests/MidiImportTests.cpp; no JUCE component or wall-clock. Parser sorts notes by start, then pitch, then source track/channel/event order; the Action inserts one clip at the supplied beat with length max(latest note end, grid subdivision).

**agent** — 2026-09-01T21:34:23Z

Completed engine-free parseStandardMidiFile and EditOps::importMidi. Tests in tests/MidiImportTests.cpp drive both seams with no JUCE component or wall-clock.

Literal format-0 PPQ, format-1 PPQ, and SMPTE fixtures produce independently stated beat/pitch/length/velocity values. Format-1 tempo/metre events are counted as unsupported and do not change the project. Malformed/empty/note-less files return a producer-facing message and the caller emits no Action. Merge order and summary text match across runs. Import MIDI inserts one clip at the supplied beat; undo is digest-exact; sourceReference stays empty.

Summary text is always "<n> note(s), <n> unmatched note-on(s), <n> unsupported event(s)." End-of-track is not counted. Zero-length paired notes are dropped. Clip length is max(latest note end, minimumLengthBeats).
