---
id: fsaacq
title: Keyboard traversal and accessible control contract
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

Make every shipping producer control representable to JUCE focus/accessibility rather than leaving essential actions as anonymous painted hit regions.

## Settled contract

- Each interactive control has a stable component id, accessible title, role, state/value where applicable, tooltip/help text, `setWantsKeyboardFocus(true)`, and a visible Graphite focus ring.
- Painted surfaces may retain paint, but each independent action/hit zone is backed by a child control or an accessibility handler with equivalent keyboard activation. Enter activates buttons/menu rows; Space toggles; arrows adjust continuous/radio controls; Escape cancels the active gesture/menu before moving focus.
- Window traversal order follows screen order: transport; Browser; arrangement headers then ruler/timeline; bottom tab and active surface; Collaborator; dialogs/windows in their own visual order. Hidden/disabled controls are skipped and focus returns to the opener when a popup/editor closes.
- Text fields retain text keys and bare-letter/standard edit shortcuts never escape them.

## Acceptance and tests

- [ ] A component-tree test walks every registered interactive id and finds non-empty accessible title, role, help, focusability, and visible-focus state.
- [ ] Keyboard-only component tests reach and activate every new workflow route from the fresh shell without mouse coordinates.
- [ ] Focus survives panel hide/show, editor/dialog close, project replacement, and error presentation predictably.
- [ ] Existing shortcut tests prove text-field suppression and focused-surface edit routing.

Use existing component tests plus a reusable accessibility inventory helper under `tests/gui`; do not add screenshots. Run all AGENTS.md checks before closing.
