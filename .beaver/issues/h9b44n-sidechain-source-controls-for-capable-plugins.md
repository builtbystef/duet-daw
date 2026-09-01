---
id: h9b44n
title: Sidechain source controls for capable plugins
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
depends_on:
    - rog54z
    - 808ncc
parent: jsfhhg
created: 2026-09-01T18:36:20Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Expose sidechain assignment beside a capable plugin's insert row. Do not put this control in the generic plugin editor: routing belongs to the Mixer graph.

## Settled interaction

- A capable insert row has a labelled `Key: <source>` control. Its menu contains None followed by cycle-safe source tracks in arrangement order; the plugin's owner and graph-cycling choices are absent.
- An incapable plugin shows no key control. A missing plugin retains a disabled current-source label so project state is explainable.
- Selecting a source is one `Set Sidechain Source` Action; None is one `Clear Sidechain Source` Action; reselect/dismiss emits none.
- Deleting source/plugin is handled by the graph-policy task and refreshes the row immediately.

## Acceptance and tests

- [ ] Capability/choices/current source are paintless Mixer facts; component code never inspects processor buses.
- [ ] Assignment, clear, cycle rejection, undo, and save/reopen are exact.
- [ ] An offline compressor fixture or built-in compressor render proves keyed gain reduction only while the source feeds the sidechain, using measured level/envelope features rather than samples.
- [ ] Component tests cover visible capable/incapable rows and keyboard menus.

Start in `Mixer`, `MixerCanvas`, and model/plugin tests. Run all AGENTS.md checks before closing.
