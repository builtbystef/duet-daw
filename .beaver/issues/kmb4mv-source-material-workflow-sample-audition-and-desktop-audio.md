---
id: kmb4mv
title: 'Source material workflow: sample audition and desktop audio/MIDI import'
state: todo
priority: high
labels:
    - spec
    - roadmap:yfpnps
parent: yfpnps
created: 2026-09-01T18:07:43Z
updated: 2026-09-01T18:42:01Z
---

## What to build

Let the Target Producer evaluate and import source material without first turning its containing directory into a permanent Browser root. Browser samples can be auditioned without changing the project. Supported audio and Standard MIDI Files can be dragged from the desktop onto compatible arrangement lanes, with an Import command as the keyboard/menu alternative.

## Acceptance criteria

- [ ] Selecting a Browser sample and pressing the visible audition control or Space plays it through the main output; pressing again, selecting another sample, closing the Browser, or replacing the project stops it promptly.
- [ ] Only one source audition plays at a time, audition follows neither the project transport nor producer undo, and playback/recording state is unchanged when it starts or stops.
- [ ] The Browser shows audition progress and the selected item's file identity; unreadable/missing files report a plain row-level error without disturbing the project.
- [ ] Dropping a supported external audio file onto an audio lane previews a valid target, copies the file into the project's `audio/` directory, and creates one snapped audio clip as one Import Audio Action.
- [ ] Dropping audio on a MIDI/group lane or outside the timeline is visibly invalid and performs no copy and no Action.
- [ ] Dropping a Standard MIDI File onto a MIDI lane creates one MIDI clip at the snapped drop beat, merging its note-bearing tracks deterministically and preserving note pitch, musical start, length, and velocity; unsupported events do not become invented project data.
- [ ] MIDI import does not change the project's tempo or metre implicitly. File timing is interpreted in musical beats against the current project, and the resulting policy is stated in the import result.
- [ ] The Import menu/file-chooser route imports onto the focused compatible track at the playhead and gives the same result as a desktop drop.
- [ ] Imported MIDI is ordinary editable project data with no enduring dependency on the source `.mid` file. Imported audio and Sampler material obey ADR 0005 and remain self-contained across Save As.
- [ ] Duplicate file names in `audio/` are resolved without overwriting different content; undo removes imported clips/notes but does not delete already copied source files.
- [ ] Large audio files and MIDI parsing do not block paint or the audio callback; the interface reports progress or a busy state and cancellation leaves no partial project edit.

## Testing seam

File classification/parsing, deterministic MIDI merge, destination validation, and import Actions are paintless. Component tests cover OS file-drag and chooser routing. Sample audition and imported audio are checked by audio features rather than stored samples.

## Notes

**agent** — 2026-09-01T18:42:01Z

AFK decomposition: collision-safe copy k0xsd3 -> worker c86xh8; Browser scan b4yf2j; source audition ws76xq; MIDI parser a5ffsn; desktop/chooser integration myodzf depends on worker+parser. This issue is a spec container.
