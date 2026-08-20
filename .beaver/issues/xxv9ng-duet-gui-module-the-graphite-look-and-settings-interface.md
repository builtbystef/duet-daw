---
id: xxv9ng
title: duet_gui module, the Graphite look, and Settings > Interface
state: done
priority: high
depends_on:
    - sea14w
parent: 535bbo
created: 2026-08-12T03:48:12Z
updated: 2026-08-20T05:50:20Z
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
- [ ] Interface scale, worked: a surface whose logical height is 24 units measures 24 px at scale 1.0 and 36 px at the default 1.5× (the default was 1.25× when this issue was written; the Target Producer raised it at review on 2026-08-19, and spec 535bbo is amended to match); changing the setting re-lays out live, with no restart.
- [ ] The Settings window opens with an Interface tab carrying theme and interface scale; both values round-trip through the app-global settings interface across a restart.
- [ ] Scrollbars are the thin workstation form and surfaces are mid-density and flat, in both modes.
- [ ] Closure waits for user review.

## Notes

**claude** — 2026-08-17T04:13:08Z

Pointer (2026-08-17): the token stylesheet is now vendored at docs/ui-tokens.css (verbatim copy of the mockup's app/globals.css @ 70122ce, with a header listing the r4m858 amendments that override it). Build the token set from that file — the out-of-repo mockup is no longer required for this slice and is unreachable from a sandbox. The mockup repo remains the pixel reference for layout and screens.

**claude** — 2026-08-19T20:42:14Z

Built (2026-08-19). All four checks pass: format, lint sweep clean, full build, 90/90 ctest.

## What landed

**The module, as two targets.** `duet::gui` is the paintless half — token set, the settings interface, the appearance view-model — and links no JUCE at all. `duet::gui_components` is the thin half: the Graphite look and feel, Inter, the Settings window. The split is the seam made into a link error: `duet_tests` links `duet::gui` and nothing graphical, exactly as it links the two facades and no engine target, so a view-model header that reached for a component or a paint type would stop compiling there. `duet_app` links `duet::gui_components` and hosts all of it.

**One JUCE configuration, split in two.** The root CMakeLists now carries `duet_juce_config` (what juce_core/graphics/gui_basics read) with `duet_engine_config` linking it and adding the engine-only definitions. Without that split the GUI target would have had to link the engine to see the same definitions — JUCE modules are INTERFACE sources, so a definition that differed between two targets is an ODR violation and not a build error.

**Tokens.** 39 colour tokens, both palettes, from `docs/ui-tokens.css` with the r4m858 amendments applied. Asserted value by value in `tests/GraphiteTokenTests.cpp`, plus: the Collaborator's teal appears in no other token in either mode; the semantic four are more than 30 degrees of hue from it and from each other; the eight track colours are distinct in both modes.

**Inter.** Compiled in with `juce_add_binary_data` (Regular and Bold; OFL.txt beside them). `setDefaultSansSerifTypeface` makes it the application typeface, so a tab label or a slider's text box is Inter too, not only the fonts the look and feel hands out. `readoutFont` asks for the `tnum` OpenType feature, which JUCE 9 exposes as `FontOptions::withFeatureEnabled`. `tests/gui/TypographyTests.cpp` measures: with tnum every digit is 6.96 px at 13 px; without it a '1' is 4.27 px against a '0' at 6.74. The shell's transport readout uses it.

**Appearance.** `resolveTheme(preference, systemIsDark)` plus a listener list. The scale is a plain `scaled(logicalUnits)`; the criterion's worked example is a test (24 → 24 px at 1.0, 30 px at the default 1.25x). Both settings are read at construction and written on change, through `duet::gui::Settings`.

**Settings, over the engine's PropertyStorage.** `duet_app`'s `PropertyStorageSettings` owns a `te::PropertyStorage`, which needs no Engine — it is the properties file under the user's application-data folder, where the engine's settings already are. Keys `interface.theme` (stored as a word, not an enum number) and `interface.scale`.

## Verified live, not only in tests

Run under X on the dev machine, screenshots taken at each step:

- First launch under a light desktop → light; `gsettings set org.gnome.desktop.interface gtk-theme Yaru-dark` on the **running** app flipped it to dark with no restart.
- Settings > Interface opened with Theme and Interface scale; choosing Light turned both windows light while the desktop stayed dark; killing and relaunching came back light, with `interface.theme="light"` in `~/.config/Duet/Settings.xml` alongside the engine's 94 other values.
- Dragging Interface scale to 1.75x re-laid out both windows where they stood, and `interface.scale="1.750000"` landed in the store.

The dev machine's Settings.xml and gtk-theme were restored afterwards.

## Decisions a reviewer should know

- **Seams.** The spec names two; this slice sits on the view-model one. Paint stays untested (spec: no screenshot or pixel tests). What `tests/gui/` asserts is measurement, not paint: that Inter is in the binary, that its digits are tabular, that a scrollbar is 9 logical units with no end buttons, that the look's colours are the token set's and follow the theme. It is the one Duet suite that links JUCE, and it is separate from `duet_tests` for that reason.
- **The generated binary-data header is marked SYSTEM** on `duet_gui_fonts`. It lands at `build/modules/duet_gui/...`, which the lint's header filter would otherwise take for one of Duet's own.
- **A JUCE fact worth carrying forward, recorded where it bites** (`SettingsWindow.cpp`): a `ComboBox` copies the look and feel's colours into its text label only when it is told the look has changed, and nothing else tells it. Changing the look and feel's colour table and repainting is not enough — the box paints the old theme's ink over the new theme's surface. This was a real defect, found by looking at the running app and not by any test; the fix is `sendLookAndFeelChange()` on the tree. Any later surface that follows the appearance needs the same call.
- **`Appearance::resolve()` notifies even when the palette does not move** — Follow OS to Dark under a dark desktop still changes what the Interface tab must show.

## Discovered, not done here

Issue **uztxbx** (blocked on this one): the app's settings store and a session's engine each open their own `juce::PropertiesFile` on the same path. Duet's side reloads before every write so it never drops the engine's keys; the engine's side does not reciprocate. Nothing has been seen to lose a setting — the shape is wrong, and it gets worse as more app-global settings land.

## What you must do

This issue's body says closure waits for user review. Run `pw-jack ./build/modules/duet_app/duet_app_artefacts/Debug/Duet`, press Settings, and judge the look in both modes and at a couple of scales. Then close this issue to approve, or leave a note with the changes you want and remove the `needs-review` label.

**claude** — 2026-08-20T02:07:59Z

Review change (2026-08-19): the default interface scale is now 1.5x, not the 1.25x this slice first shipped. The Target Producer ran the shell, found 1.25x still small, and chose 1.5x. r4m858 had settled 1.25x against the prototype, so this amends spec 535bbo — its Visual language clause and a note there now say so, and docs/ui-tokens.css carries the same amendment in its header. The number lives in exactly one place in code, duet::gui::defaultInterfaceScale.

Two tests moved with it: the worked example is now 24 logical units -> 36 px at the default (24 px at 1.0 unchanged), and the two cases that used to set 1.5x explicitly now use 1.75x, because setting the value that is already the default is no change to make and the listener would rightly not fire. This issue's acceptance criterion has been reworded to the new number with the amendment named in it.

Checked afterwards: format clean, lint clean, 90/90 ctest, and the shell run at the new default. One thing seen and not a defect: the window manager reopened Duet maximized once, having remembered that state from the earlier session; relaunching after un-maximizing gave the expected 930x495 (620x330 logical). Window geometry is app-global state a later slice owns.

**claude** — 2026-08-20T05:50:20Z

Approved and closed by the Target Producer (2026-08-20). The needs-review label came off with the closure — the review it marked is done.
