---
id: np7wjh
title: Actionable empty, busy, unavailable, and error states
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
depends_on:
    - 7sd7k2
    - hs7owx
    - otk1nr
    - 9mnzeh
    - e1stae
    - oy5ubt
    - h9b44n
    - rl41d9
    - 9jbaki
    - nt104h
    - jg62kc
    - jk80m7
    - ehdor9
    - ws76xq
    - myodzf
    - b4yf2j
parent: kkclj0
created: 2026-09-01T18:38:30Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Complete local status presentation through one small engine-free status shape (`idle/busy/empty/unavailable/error`, message, optional action) rather than ad-hoc silent no-ops.

## Required state catalogue

- Arrangement: no clips and invalid import target.
- Piano Roll: no MIDI clip open and no selected notes for a command.
- Browser/import: empty sample folder, scanning, unreadable folder/file, audition loading/error, import progress/canceled/failed.
- Track I/O: None, unavailable input, incompatible/failed arm.
- Mixer: no groups for output/send, no sidechain choices, impossible route.
- Plugin/editor: no plugins scanned, missing plugin, editor unavailable, missing Sampler zone.
- Collaborator/export/project: no provider, no Suggestions, Collaborator failure, save/export failure.

## Settled presentation

A state appears beside the control/surface that owns it, says what happened and the next useful action, replaces prior state when recovered, never opens repeated modal dialogs, and never disables unrelated DAW work. Raw ids/paths may appear only when they are the producer's chosen file identity.

## Acceptance and tests

- [ ] Every catalogue state has one reproducible model/component test and exact producer text approved against `docs/GLOSSARY.md`.
- [ ] Recovery clears the state; retries do not stack duplicate notices or Actions.
- [ ] Failure/cancellation tests preserve project digest unless the documented successful Action already committed.

Start from existing `onEngineMessage`/local status patterns and keep ownership at the narrowest surface. Run all AGENTS.md checks before closing.
