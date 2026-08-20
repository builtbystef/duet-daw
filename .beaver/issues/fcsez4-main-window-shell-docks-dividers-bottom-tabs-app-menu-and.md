---
id: fcsez4
title: 'Main window shell: docks, dividers, bottom tabs, app menu, and the VIEW tree'
state: done
assignee: claude
priority: high
depends_on:
    - xxv9ng
    - 1c8sjh
parent: 535bbo
created: 2026-08-12T03:48:32Z
updated: 2026-08-20T09:40:49Z
---

## What to build

The single main window the whole interface lives in. A transport-bar strip across the top (empty of controls until its own slice), the arrangement in the center, a browser dock left, the Collaborator dock right, and a resizable, collapsible bottom panel with Piano Roll and Mixer tabs. Draggable dividers separate all three docks from the arrangement. One "Duet" app menu button replaces any menu bar; it carries the panel toggles here, and project commands arrive with the lifecycle slice. Plugin editors will be the only floating windows; nothing else tears off.

This slice also establishes the two places UI state lives. Window geometry is app-global. Per-project view state is a `VIEW` child of the DUET tree, written with no UndoManager so it never reaches producer undo, and captured at save time so that resizing a panel never dirties the document. The shape, extended by later slices with their own attributes:

```
DUET
└─ VIEW  (covered by duetSchemaVersion like the rest of DUET)
   ├─ @hZoomPxPerBeat, @hScrollBeats, @vScrollPx
   ├─ @browserVisible, @collaboratorVisible, @bottomVisible
   ├─ @browserWidthPx, @collaboratorWidthPx, @bottomHeightPx, @bottomTab ("pianoRoll"|"mixer")
   └─ TRACKVIEW (one per track)  @trackRef, @heightPx, @lanesExpanded
```

Keyboard dispatch policy lands here too, because this is the first surface with keys: bare-letter shortcuts are inactive while a text field has focus, and later slices register their keys through the same policy.

Prototype finding worth the implementer's time (r4m858): a `setBounds` call with unchanged bounds skips the resize callback, so switching the bottom tab must refresh the newly visible surface explicitly.

## Acceptance criteria

- [ ] One main window shows the transport strip, the arrangement, both side docks, and the bottom panel; dragging a divider resizes the neighbouring dock, and each dock can collapse and reopen at its previous size.
- [ ] The Duet menu button opens a menu carrying the panel toggles; there is no menu bar and no floating panel.
- [ ] Keys B, C, E toggle the browser, Collaborator, and bottom panel; P and X select the Piano Roll and Mixer tabs. With a text field focused, those same bare letters type their character and toggle nothing.
- [ ] Switching the bottom tab shows the newly selected surface correctly laid out, including when the panel's bounds are unchanged by the switch.
- [ ] View round-trip, worked: set browser width 260 px, Collaborator hidden, bottom panel 320 px on the Mixer tab; save, reopen the project → the layout returns with those exact values, and the reopened document is not dirty.
- [ ] A VIEW write never appears on the undo stack: collapse a dock, resize it, switch tabs, then undo — the last edit Action is what undoes, and the layout does not change.
- [ ] Changing only view state and closing the project prompts nothing; the document is dirty only if an Action was performed.
- [ ] Window position and size are app-global: they restore on relaunch regardless of which project opens.
- [ ] Surfaces render on the software renderer by default, with a per-surface hardware-accelerated context available as an opt-in escape hatch that changes nothing visible when enabled.

## Notes

**claude** — 2026-08-20T09:40:45Z

Done. The main window shell, the VIEW tree, the keyboard policy, and the rendering hatch.

**What was built**

- `duet::gui::ViewState` (paintless) — the per-project view: zoom and scroll, which docks are open and how wide, which bottom tab is in front, a row per track. Every layout gesture goes through it rather than around it, so the layout is the project's.
- `duet::persistence::DataNode` — a named node with named values and named nodes under it, text all the way down. It is how Duet's own project data crosses the persistence facade with no engine and no JUCE in the way, and it is what `ViewState` serialises to.
- `Project::onCaptureViewState` / `Project::viewState` — the view is asked for at one moment only, as a save begins, and written into DUET/VIEW with no undo manager.
- `duet::gui::Shortcuts` (paintless) — the keyboard policy. A key with no Ctrl and no Alt is a bare key, and a bare key is the producer typing whenever a text field has focus. Later slices add their enumerators and register with the same table.
- `duet::gui::WindowGeometry` and `duet::gui::Rendering` (paintless) — the two app-global answers the shell needs.
- `duet::gui::MainShell` (components) — the window: transport strip with the Duet menu button, arrangement, browser and Collaborator docks, three draggable dividers, the collapsible bottom panel with its Piano Roll and Mixer tabs. The docked surfaces are empty placeholders that carry their name, and each later slice replaces one.
- `duet::gui::AcceleratedSurface` (components) — the escape hatch, an `OpenGLContext` over one surface; `juce::juce_opengl` joins `duet_gui_components`.
- `duet_app` hosts the shell, restores and stores the window geometry, and hands the open project's view to the persistence facade.

**Decisions made**

1. **The VIEW tree's Px attributes are real pixels, not logical units.** The attribute names are the spec's and they say Px; `hZoomPxPerBeat` has to be real pixels for `TimelineGeometry` (slice 5he6vd) to map beats to x without a scale factor, and a dock is whatever width the producer dragged it to. The shell's own chrome — strip heights, tab bar, divider thickness — stays in logical units through `Appearance::scaled`, as everything in this module does.
2. **The VIEW tree's name lives in `duet_persistence`** (`viewTreeName`), not in the view-model. It is part of the project format. TRACKVIEW, which is the shape inside VIEW, stays with `ViewState`.
3. **`DataNode` rather than a flat key/value under DUET.** `setDuetValue` already exists and dirties the project by design; the view must not, and VIEW has children, which a flat store has no room for.
4. **The throwaway demo shell in `duet_app` is gone**, replaced by the real window. Its vocabulary walk was already covered independently by `DemoWalkthroughTests`. New/Open/Save and Play/Stop stayed, moved into the Duet menu under the panel toggles and marked as scaffolding: the lifecycle flow is ce17ym's and the transport is 1fumn6's, and without them the app could neither open a project nor be heard for several slices.
5. **The Duet menu's host entries** come through `setHostMenu`, with ids from `MainShell::firstHostMenuId` up. The shell knows panels; the host knows projects. That is the seam ce17ym plugs into.
6. **The rendering hatch is opt-in from Settings > Interface**, a third row on the tab, and it applies where it stands. `SettingsWindow` gained the app-global store and a callback for it.
7. **A third copy of the in-memory `Settings` double was not made.** It moved into `duet::test_support` for the paintless suite, and the JUCE-linked suite — which deliberately links no facade — got `tests/gui/GuiTestSupport.h`.

**Facts a reviewer needs**

- The r4m858 finding is real in this code and the test bites: comment out `bottom->layOutSurfaces()` in `MainShell::viewStateChanged` and `switching the bottom tab lays the newly shown surface out, bounds or no bounds` fails on `mixer.isVisible()`. A tab switch moves nothing, so the panel's `resized()` never runs.
- Criteria verified in the running app, not only in tests: the five areas laid out (screenshot); the window reopening at exactly the geometry it stored (`interface.windowBounds` = `160 69 1600 957`, restored on the next launch); and the hardware context on, which produced a pixel-identical window.
- The keyboard's negative case — bare letters typing rather than toggling — is asserted at the policy, which is where the rule lives. At the component the answer comes from `juce::Component::getCurrentlyFocusedComponent()`, and JUCE gives a focused text field the key before the shell sees it at all.
- `defaultInterfaceScale` is 1.25x again: the Target Producer withdrew the 1.5x amendment at this slice's review, having now seen the whole window. Recorded on spec 535bbo, in `docs/ui-tokens.css`, and in `Appearance.h`.
- Published issue ax88i4: `a take starts at the bound even if the devices never settle` failed twice in a full sweep while the machine was contending for the audio device, and is timed against a 5 ms pump with no margin. Not caused by this slice — the same sweep passes twice in a row on this tree, and passed on the tree without it.

All four checks pass: format clean, lint clean, 134/134 tests, and the app runs.
