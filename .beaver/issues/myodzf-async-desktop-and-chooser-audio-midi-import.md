---
id: myodzf
title: Async desktop and chooser audio/MIDI import
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
depends_on:
    - k0xsd3
    - a5ffsn
    - c86xh8
parent: kmb4mv
created: 2026-09-01T18:34:33Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Connect OS file drops and `Duet > Import…` to the existing arrangement, collision-safe audio copy, and deterministic MIDI importer. All classification/read/copy/parse work runs on a worker; only the final validated Action is marshalled to the message thread.

## Settled interaction

- Supported audio previews valid only on audio tracks; `.mid`/`.midi` previews valid only on MIDI tracks. Group, Master, wrong-kind, and outside-timeline targets show the invalid-drop cursor and do nothing.
- A desktop drop starts at the pointer's snapped beat with Alt bypass. `Import…` targets the focused compatible track at the playhead; with no compatible focus it asks Audio track or MIDI track and creates that track plus import in one Action.
- While working, the target shows file name and progress/busy state. Escape or project replacement cancels. A canceled/failed job emits no project Action and deletes only its own partial copy.
- Before commit, validate that the original project generation, target track, and intended beat still exist. A stale result reports `Import canceled because the project changed` rather than landing elsewhere.
- Audio clip length comes from the imported file reader. MIDI uses the parser's summary, shown once in local status.

## Acceptance and tests

- [ ] Desktop drop and chooser produce equivalent clips for the same destination.
- [ ] Wrong targets, malformed files, worker failure, cancellation, and project replacement leave project digest and undo unchanged.
- [ ] A commandable executor proves paint/message responsiveness and stale-result rejection without sleeps.
- [ ] Component tests cover JUCE file-interest/drop/chooser routing; model/persistence tests cover resulting content.

Start in `ArrangementCanvas`, `MainShell`, `Main.cpp`, and the host-owned project import wiring. Run all AGENTS.md checks before closing.
