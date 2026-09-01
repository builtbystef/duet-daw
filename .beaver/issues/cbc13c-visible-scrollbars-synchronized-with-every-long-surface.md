---
id: cbc13c
title: Visible scrollbars synchronized with every long surface
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

Add thin Graphite `juce::ScrollBar` controls wherever content can exceed its viewport, synchronized with the existing view-model scroll values.

## Required bars

- Arrangement: horizontal timeline and vertical tracks.
- Piano Roll: horizontal timeline and vertical keys.
- Mixer: horizontal strips and vertical content where send/insert rows overflow.
- Browser and built-in/Sampler editors: vertical lists.
- Collaborator history/conversation: existing scroll behavior must expose its position too.

## Settled behavior

Bars show range/current viewport, drag live, hide only when all content fits, remain keyboard reachable, and do not change wheel conventions. Ruler, track headers, piano keyboard, Master strip, and command strips remain pinned as appropriate. Scroll values continue to persist only where existing `ViewState` says they do.

## Acceptance and tests

- [ ] Literal range/viewport tests prove each bar and wheel/zoom gesture update the same model value both directions without feedback loops.
- [ ] Content shrink clamps positions and hides bars; interface-scale changes recompute viewport without losing the musical anchor.
- [ ] Component tests cover composition and pinned regions, not pixels.

Run all AGENTS.md checks before closing.
