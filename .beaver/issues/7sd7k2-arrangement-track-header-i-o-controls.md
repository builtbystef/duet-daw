---
id: 7sd7k2
title: Arrangement track-header I/O controls
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
depends_on:
    - oscfrz
    - 808ncc
parent: uxkosp
created: 2026-09-01T18:33:36Z
updated: 2026-09-01T18:41:15Z
---

## Bounded implementation

Put the shared Track I/O state in every arrangement track header without redesigning the timeline. Use the paintless `TrackIo` seam from the blocker and thin JUCE controls/hit zones in `ArrangementCanvas`.

## Settled layout and interaction

- The header has two compact labelled rows beneath the track name: `In: <choice>` and `Out: <choice>`. The input row's menu also contains `Monitor: Off / While Armed / On`; output opens its cycle-safe destinations.
- The existing `R` hit target remains visible beside M/S, but groups show no R and their input row reads `Bus (sum)` rather than a dead picker.
- While a take rolls, an armed track's R control reads as recording with the semantic-danger token; an armed but idle track uses the existing active treatment.
- Unavailable and None are full text states, not disabled blank controls. Menus are keyboard operable, Escape dismisses without a write, and every target has an accessible name and tooltip.

## Acceptance and tests

- [ ] A newly added audio or MIDI track can choose input, monitoring, output, and arm from its header.
- [ ] The labels refresh after a change from either surface or a device rebuild.
- [ ] Group, unavailable, None, armed, and recording states render distinct component state.
- [ ] Re-selecting a value and dismissing a menu emit no Action/configuration write.
- [ ] Component tests drive hit routing, menu choices, focus, names, and mirrored refresh; paint is not asserted.

Start in `ArrangementView.h/.cpp`, `ArrangementCanvas.h/.cpp`, and `tests/gui/MainShellTests.cpp`; add a focused GUI test only if that is the outermost observable seam. Run all AGENTS.md checks before closing.
