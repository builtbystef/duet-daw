---
id: xxv9ng
title: duet_gui module, the Graphite look, and Settings > Interface
state: todo
priority: high
depends_on:
    - sea14w
parent: 535bbo
created: 2026-08-12T03:48:12Z
updated: 2026-08-12T03:48:12Z
---

## What to build

The fifth module, `duet_gui`, joins the foundation's three: it depends on the model and persistence facades and is hosted by the app shell. It lands with the seam every later surface follows — a paintless view-model holding geometry, arithmetic, selection, and state, plus a thin component that only paints and forwards events — and with the Catch2 harness that attaches to view-models.

Its first visible product is the Graphite visual language: one token set carrying both palettes, the bundled typeface, and the interface scale, plus the app-global settings store behind a small settings interface the app shell implements (over the engine's PropertyStorage, which the foundation already initializes). The Settings window ships here with its Interface tab — theme and interface scale — and later slices add their own rows and tabs to it.

Standing visual reference: the Target Producer's mockup repo at `~/Code/temp/duet-daw-ui-mockup`, whose token stylesheet is the token source of truth, amended by the prototype findings recorded in spec 535bbo. Where the mockup and the spec disagree, the spec wins.

## Acceptance criteria

- [ ] A `duet_gui` module target builds and is hosted by the app shell; its view-model headers pull in no component or paint types, and a Catch2 test exercises a view-model with no window on screen.
- [ ] One token set carries both palettes: dark mode's text steps are the amended values (primary `#d2d2d2`, secondary `#9e9e9e`, muted `#828282`, disabled `#5c5c5c`) with dark surfaces, borders, and track colors unchanged; light mode matches the mockup exactly.
- [ ] The Collaborator accent is a single named token pair (dark `#4aa294`, light `#0e7c70`) with no second definition; semantic info/success/warning/danger keep their own hues.
- [ ] Eight desaturated, user-assignable track colors exist as tokens in both modes.
- [ ] Inter ships inside the binary as the application typeface, and a numeric readout renders with tabular numerals (the same digit string occupies identical width as its digits change).
- [ ] Theme resolution, worked: first launch with no stored preference under an OS set to dark → dark; the producer chooses Light → light survives a restart even while the OS says dark; the producer chooses Follow OS → an OS flip changes the theme without a restart.
- [ ] Interface scale, worked: a surface whose logical height is 24 units measures 24 px at scale 1.0 and 30 px at the default 1.25×; changing the setting re-lays out live, with no restart.
- [ ] The Settings window opens with an Interface tab carrying theme and interface scale; both values round-trip through the app-global settings interface across a restart.
- [ ] Scrollbars are the thin workstation form and surfaces are mid-density and flat, in both modes.
- [ ] Closure waits for user review.
