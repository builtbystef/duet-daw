---
id: 1bo4s7
title: Pointer cursors, hit targets, and Graphite density sweep
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

Apply one mechanical pointer/hit-target policy to all finished surfaces and remove residual default/clipped chrome.

## Settled policy

- Pointing hand: button/menu/link; I-beam: editable text; open/closed hand: draggable body; left-right/up-down: trim/resize/divider/fader as appropriate; forbidden: invalid drop/route; normal: inert drawing.
- Every pointer target is at least 18x18 logical pixels, even when its visible glyph is smaller. Overlapping zones choose the most specific operation: loop handle, trim edge, body, empty surface.
- Controls use Graphite LookAndFeel/tokens in dark and light at every supported interface scale. Text may ellipsize only with full tooltip; no raw ref, default JUCE grey, clipped label, or overlapping target remains.
- Teal remains reserved for Collaborator/Suggestion state; producer drag/selection feedback uses existing non-teal tokens.

## Acceptance and tests

- [ ] A table-driven hit/cursor test covers each operation class and overlap precedence at minimum/default/maximum scale.
- [ ] Component bounds tests cover minimum targets and no overlap in minimum supported window/panel sizes.
- [ ] A source grep/inventory finds no newly introduced literal UI colour or unexplained single-letter control.

Run all AGENTS.md checks before closing; visual judgment remains in the dependent review issue.
