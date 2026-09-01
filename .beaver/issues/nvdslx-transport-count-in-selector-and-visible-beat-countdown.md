---
id: nvdslx
title: Transport count-in selector and visible beat countdown
state: todo
priority: low
labels:
    - session:task
    - roadmap:h0eir5
depends_on:
    - 04qcp0
parent: 3xxk8b
created: 2026-09-01T18:40:41Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Expose count-in beside Record in the transport and distinguish counting from capture.

## Settled interaction

- A labelled `Count-in: Off/1 bar/2 bars` popup sits beside the metronome/Record controls and has a full tooltip. Selection updates project view/transport state immediately with no Action.
- Pressing Record during idle starts the native count. During counting, Record uses an amber pre-record state and the transport readout shows `Count-in — bar N, beat M` from model phase/remaining beats. At capture it turns semantic-danger red and reads Recording.
- Pressing Record again, Stop, or Escape during counting cancels. During recording, existing Record/Stop behavior lands the take.
- Project replacement, device failure, and window close clear countdown state and never leave Record lit.

## Acceptance and tests

- [ ] TransportBar/model tests cover choices, labels, phase transitions, cancel commands, project replacement, and no-undo persistence.
- [ ] Component tests cover focus/order/tooltips/state composition; countdown timing is supplied by the model, never a UI timer guess.

Start in `TransportBar`, `MainShell::TransportStrip`, ViewState serialization, and transport tests. Run all AGENTS.md checks before closing.
