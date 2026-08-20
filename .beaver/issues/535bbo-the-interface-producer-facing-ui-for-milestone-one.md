---
id: 535bbo
title: The Interface — producer-facing UI for milestone one
state: todo
priority: high
labels:
    - spec
depends_on:
    - s11o4w
    - r4m858
created: 2026-08-10T22:39:45Z
updated: 2026-08-20T09:30:40Z
---


## Problem Statement

The milestone-one DAW core (spec b1j3me) and the Collaborator (spec js437t) are fully specified, but the Target Producer has no surface to work on: nothing to arrange in, no piano roll to edit notes in, no mixer, no place where a Suggestion becomes visible. Every settled capability is invisible until the interface exists.

## Solution

A single main window in the "Graphite" visual language: an achromatic workstation where contrast carries emphasis and the one reserved hue — teal — always and only means the Collaborator. The arrangement timeline is the central surface, with a browser dock left, the Collaborator panel right, and a resizable bottom panel holding the Piano Roll and Mixer. One smart tool edits everything; snap, zoom, and selection behave the same on every surface. Dark and light modes ship as equals. The standing visual reference is the Target Producer's Next.js mockup with the amendments verified live at the UI prototype; this spec records the decisions, and those two artifacts record the pixels.

## User Stories

1. As the Target Producer, I want one window with dockable panels, so that arranging, editing, mixing, and talking to the Collaborator never require window management.
2. As the Target Producer, I want to arrange clips on a timeline with snap, adaptive grid, and one smart tool, so that moving, copying, trimming, and looping need no tool palette.
3. As the Target Producer, I want a piano roll with velocity, quantize, scale highlighting, and Fold, so that I can edit MIDI precisely.
4. As the Target Producer, I want a mixer with faders, pan, mute/solo, meters, I/O routing, and insert chains, so that I can mix without leaving the bottom panel.
5. As the Target Producer, I want to draw automation in expandable lanes under tracks, so that parameters move over time.
6. As the Target Producer, I want a browser with search and favorites over my samples, the built-in devices, and my VST3 plugins, so that inserting an instrument or effect is one drag.
7. As the Target Producer, I want Suggestions rendered as teal ghosts in place — on the timeline, in the mixer — so that I can see and Audition exactly what the Collaborator wants to change before it enters the project.
8. As the Target Producer, I want dark and light modes that follow my OS on first launch, so that the app fits my environment.
9. As the Target Producer, I want my view (zoom, scroll, panel layout, track heights) restored when I reopen a project, so that I resume where I left off.
10. As the Target Producer, I want to hit record in a brand-new untitled project without saving first, so that no dialog stands between me and a take.
11. As the Target Producer, I want keyboard shortcuts for transport and panels, so that my hands stay on the work.

## Implementation Decisions

### Module shape

A new module, **`duet_gui`**, joins the foundation's three. It depends on `duet_model` (every edit gesture ends in `performAction`; ADR 0004) and `duet_persistence` (project lifecycle, the DUET tree), and is hosted by `duet_app`, which keeps the shell duties (message loop, device management, transport ownership). `duet_gui` never touches engine or JUCE-engine types through the model — it sees only the vocabulary layer's interface.

Every surface splits along the agreed test seam:

- a **view-model** — a plain C++ class holding geometry, hit-testing, snap and grid arithmetic, selection, and serialization; no `juce::Component`, no paint, no engine types;
- a thin **component** — a `juce::Component` that paints the view-model's state and forwards input events to it.

Representative contracts at the seam (the shapes are the decision; names may be refined in place):

```cpp
// Time↔pixel mapping and the adaptive grid. Pure; no side effects.
class TimelineGeometry {
public:
    double xToBeats (int px) const;
    int    beatsToX (double beats) const;
    // Finest subdivision whose spacing is >= 18 px at the current zoom
    // (bar / beat / fine line weights from the token set).
    GridSpec gridFor() const;
    // Zoom anchored at a pointer: the beat under anchorPx is invariant.
    void zoomAt (int anchorPx, double factor);
};

// Snap: applied on drag/trim/create; bypassed while Alt is held.
double snapBeats (double beats, GridSpec grid, bool altHeld);

// Gesture handlers end in the vocabulary layer, never in the engine:
// e.g. a completed clip drag emits one performAction("Move Clip", ops).
```

### Layout

- Single main window; **plugin editors are the only floating windows** (chrome per the mockup: bypass, save preset, close).
- **Transport bar** (top): bar/beat + wall-time readout, BPM, time signature, grid-size control, loop / metronome / follow-playhead toggles, CPU% + health indicator, undo/redo, project name.
- **Arrangement** (center): linear timeline; automation as expandable lanes under tracks.
- **Browser** (left dock): search + favorites; sections — samples from user-chosen folders, the two built-in instruments, the three built-in effects, the scanned VST3 list; drag-to-track/timeline inserts.
- **Collaborator panel** (right dock): behavior per spec js437t; placement and styling here.
- **Bottom panel**: resizable/collapsible; tabs Piano Roll and Mixer.
- Draggable dividers between all three docks and the arrangement.
- One **"Duet" app menu** — New/Open/Save/Save As/Recent, Export/Bounce, Audio & MIDI Settings, Plugin Scan, panel toggles. No File/Edit menu bar.
- **Dialogs**: Export/Bounce (name, destination, format, bit depth, sample rate, range, normalize); Audio & MIDI Settings (Audio/MIDI/Interface tabs); the plugin-scan flow (out-of-process scanning per b1j3me).

### Interaction conventions

- **Snap** on by default; grid adapts to zoom; visible grid-size control; **hold Alt bypasses mid-drag**.
- **Scroll**: plain = vertical; Shift = horizontal; Ctrl = h-zoom anchored at pointer; Ctrl+Shift = v-zoom (track/key heights); `+`/`-`/`0` keys and zoom-to-fit.
- **Adaptive grid**: the finest subdivision with **≥ 18 px** spacing (verified at r4m858).
- **Selection**: click; rubber-band on empty; Ctrl+click toggles; Shift+click extends; Ctrl+A per focused surface; Escape clears. One current selection feeds the Collaborator context chip (js437t).
- **Clips**: drag = move (snapped); Ctrl+drag = copy; edge = trim; loop-handle extends; double-click MIDI clip opens the piano roll; double-click an empty MIDI lane creates a one-grid-unit clip; Delete deletes.
- **Single smart tool everywhere** — no tool palette.
- **Piano roll**: scale highlighting, Fold, note-length control, double-click adds/removes notes, velocity editing per the mockup.
- **Automation**: double-click adds a point, drag moves it (snapped horizontally), right-click removes it; **linear segments only** in milestone one. The vocabulary layer must not foreclose per-point curvature (the engine supports it); curves are milestone-two work.
- **Context menus** (mirror vocabulary Actions only; nothing exists solely in a menu): clip — Cut/Copy/Paste/Duplicate/Delete/Rename/color; note — Delete/Quantize; empty timeline — Paste; track header — Rename/Duplicate/Delete/color picker.
- **Track lifecycle**: a "+ Add Track" row pinned under the last track header (choice: Audio/MIDI); drag headers to reorder; double-click name to rename; header controls (mute, solo, arm, resize) per the mockup.
- **Keys**: Space play/stop · R record · L loop · M metronome · Home/End · B browser · C collaborator · E bottom panel · P piano-roll tab · X mixer tab · F follow · Ctrl+Z / Ctrl+Shift+Z · Ctrl+S. Bare-letter keys are inactive while a text field has focus.

### Visual language

- **Graphite theme, both modes** (OS preference on first launch; switchable in Settings > Interface). The token set and both palettes are the mockup's `app/globals.css`, with the r4m858 amendments:
  - dark-mode text brightened one step: primary `#d2d2d2`, secondary `#9e9e9e`, muted `#828282`, disabled `#5c5c5c`; dark surfaces, borders, and track colors unchanged; light mode exactly as the mockup.
- **Collaborator accent — teal, reserved exclusively**: dark `#4aa294`, light `#0e7c70`. Used for ✦ badges, suggestion ghosts/glow, ghost fader handles, commentary accents; never anywhere else. Semantic info/success/warning/danger keep their own hues (stale stays amber).
- **8 desaturated user-assignable track colors** (token set).
- **Inter** (OFL), bundled, tabular numerals for time displays. Thin workstation scrollbars. Mid-density flat surfaces.
- **Interface scale**: global setting in Settings > Interface, **default 1.25×** (the mockup's pixel sizes read too small at 1:1; r4m858 verified that and settled 1.25×). The Target Producer raised the default to 1.5× at the review of slice xxv9ng (2026-08-19) and took that back at the review of slice fcsez4 (2026-08-20), having seen the whole main window at both.

### Suggestion rendering (approved at r4m858)

- Ghost clips: teal fill ~12% alpha, dashed teal border, 3-ring soft glow; on Audition the fill intensifies to ~26% with a solid border. Ghost clip names carry a teal ✦ prefix.
- Mixer: ghost fader as a translucent teal handle with glow; a per-strip "A: CURRENT / B: PROPOSED" chip visible only while auditioning.
- Suggestion card: teal glow border; per-element cherry-pick checkboxes; Audition / Accept / Reject buttons (the button says **"Audition"**, per the glossary). Elements excluded by cherry-pick render at ~35% intensity.
- Mechanics (cherry-pick, stale-amber, redo-against-current, rejection-with-reason, History section) are js437t's; this spec only styles them.

### Persistence of UI state

- **App-global** — theme, interface scale, window geometry, autosave interval, browser sample folders, favorites — lives in the engine's PropertyStorage (which b1j3me already requires Duet to initialize), behind a small settings interface `duet_app` implements.
- **Per-project view state** — h/v zoom, scroll position, panel visibility and sizes, expanded automation lanes, active bottom tab, track heights — lives in a `VIEW` child of the DUET tree (ADR 0005), written with nullptr-UndoManager writes (invisible to producer undo, per the foundation's per-write visibility rule) and **captured at save time**: reopening restores the view, scrolling never dirties the document.

VIEW tree shape (a schema is a decision; ValueTree sketch):

```
DUET
└─ VIEW  duetSchemaVersion covers this tree like the rest of DUET
   ├─ @hZoomPxPerBeat, @hScrollBeats, @vScrollPx
   ├─ @browserVisible, @collaboratorVisible, @bottomVisible
   ├─ @browserWidthPx, @collaboratorWidthPx, @bottomHeightPx, @bottomTab ("pianoRoll"|"mixer")
   └─ TRACKVIEW (one per track)  @trackRef (EditItemID), @heightPx, @lanesExpanded
```

### Project lifecycle behavior

- **Launch**: reopen the last project when one exists and its folder is still present; otherwise create a new untitled project. No start screen.
- **An untitled project is a real project folder from the moment it exists**, created in a default projects directory (default `~/Music/Duet Projects/Untitled N`, location configurable in Settings), so recording and the in-folder autosave/recovery mechanism (b1j3me) work before any save. First Save As renames/relocates the folder.
- **Save As copies the whole project folder** (edit file + `audio/`) and the session continues in the copy — a project stays a self-contained folder (ADR 0005).
- **Close with unsaved changes**: Save / Discard / Cancel prompt.
- **A new project seeds** one instrument track holding the built-in synth and one audio track.
- **Autosave** is a setting — off/2/5/10 minutes, **default 10** (amends b1j3me's fixed 5; recovery mechanism unchanged; the s11o4w decision).

### Rendering substrate

JUCE 9 software renderer by default — validated at 60-track density (ddp1qt) — with a per-surface `OpenGLContext` escape hatch. Duet runs under XWayland; no native Wayland work.

## Dependencies

- **Inter** (SIL OFL 1.1) — bundled as the application typeface; tabular numerals for time displays. No other new libraries: everything else is JUCE 9, already present.

## Testing Decisions

Two seams, agreed for this area:

1. **The Action seam** (exists; ADR 0004): gesture handlers are driven directly and the test asserts the emitted Actions/ops on `duet_model`'s engine-free interface — e.g. a completed clip drag emits exactly one `performAction("Move Clip", …)`; an Alt-held drag emits unsnapped positions; Ctrl+drag emits a copy, not a move.
2. **The view-model seam** (new, this spec): Catch2 tests attach to the paintless view-models. External behavior only — geometry, grid, snap, selection, serialization; never paint.

Worked examples (4/4, 120 BPM unless stated):

- **Adaptive grid**: at 20 px/beat, beat lines are 20 px ≥ 18 → grid is beats; quarter-beat lines would be 5 px < 18. At 80 px/beat, quarter-beats are 20 px ≥ 18 → grid is 1/16 notes.
- **Snap**: grid = 1 beat, drag lands at 3.30 beats → 3.0; same drag with Alt held → 3.30.
- **Zoom at pointer**: with the beat 8.0 under x=400, any `zoomAt(400, f)` keeps beat 8.0 under x=400.
- **VIEW round-trip**: serialize a view-model's state to the VIEW tree, load into a fresh view-model → equal state; a VIEW write never appears on the undo stack.
- **Selection**: click A → {A}; Ctrl+click B → {A,B}; Ctrl+click A → {B}; Escape → {}.

Prior art: none — this module lands with the first product code. The prototype branch (`prototype/milestone-ui`) is reference, not test substrate; it is disposable by definition.

## Out of Scope

- Curved automation segments (linear-only milestone one; the data model stays curve-ready).
- Screenshot/pixel tests — look was settled live at r4m858; paint code stays untested.
- CLAP hosting UI, comping/punch-in/loop-recording UI, recorded-automation modes, session/clip-launch grid, pre-fader sends UI, external hardware routing UI — all milestone-two-or-later (kimula, u24m3x).
- A third built-in instrument or effects beyond EQ/compressor/reverb — the mockup's extras are set dressing (s11o4w).
- Native Wayland windowing (hvv3nn).
- Floating/tearable panels beyond plugin editors.
- Violet/purple anywhere near the Collaborator; the bright `#3fd0be` teal on dark (r4m858).

## Further Notes

- **Standing references**: the mockup repo (`~/Code/temp/duet-daw-ui-mockup`, its own git repo — `app/globals.css` is the token source of truth) and the prototype branch `prototype/milestone-ui` (directory `prototype-ui/`). Where this spec and the mockup disagree, this spec and the r4m858 amendments win.
- **Prototype-level findings that save an implementer time** (r4m858): JUCE artefact output lands in `build/duet_ui_prototype_artefacts/`; a same-bounds `setBounds` skips `resized()`, so tab-visibility changes must be refreshed explicitly.
- The message thread is the sole writer of the project model (b1j3me); all view-models live on the message thread. Meters/playhead repaint from atomics the engine publishes — no locks in paint.
- Glossary discipline: Collaborator, Suggestion, Audition, Action, Duet Loop, Target Producer — as defined in the glossary; the Audition button label is "Audition".

## Notes

**claude** — 2026-08-17T04:13:08Z

Pointer (2026-08-17): the mockup's token source of truth (app/globals.css) is vendored at docs/ui-tokens.css with the r4m858 amendments documented in its header, so the visual tokens travel with this repository. The mockup repo and branch prototype/milestone-ui remain the standing references for layout, screens, and pixel judgment.

**claude** — 2026-08-20T02:03:31Z

Amendment (2026-08-19, Target Producer, at the review of slice xxv9ng): the default interface scale is 1.5x, not 1.25x. r4m858 settled 1.25x against the prototype; seeing the shell run at that scale, the Target Producer asked for a larger default. Everything else about the setting is unchanged — the range, the live re-layout, and the Settings > Interface row. The spec body's Visual language section has been updated to match; docs/ui-tokens.css carries the same amendment in its header, and duet::gui::defaultInterfaceScale is the one place the number lives in code.

## Notes

**claude** — 2026-08-17T04:13:08Z

Pointer (2026-08-17): the mockup's token source of truth (app/globals.css) is vendored at docs/ui-tokens.css with the r4m858 amendments documented in its header, so the visual tokens travel with this repository. The mockup repo and branch prototype/milestone-ui remain the standing references for layout, screens, and pixel judgment.

**claude** — 2026-08-20T02:03:31Z

Amendment (2026-08-19, Target Producer, at the review of slice xxv9ng): the default interface scale is 1.5x, not 1.25x. r4m858 settled 1.25x against the prototype; seeing the shell run at that scale, the Target Producer asked for a larger default. Everything else about the setting is unchanged — the range, the live re-layout, and the Settings > Interface row. The spec body's Visual language section has been updated to match; docs/ui-tokens.css carries the same amendment in its header, and duet::gui::defaultInterfaceScale is the one place the number lives in code.

**claude** — 2026-08-20T09:30:40Z

Amendment (2026-08-20, Target Producer, at the review of slice fcsez4): the default interface scale goes back to 1.25x, which is what r4m858 settled. The 1.5x amendment of 2026-08-19 (review of xxv9ng) is withdrawn — the Target Producer asked for it having seen the shell of that slice, and took it back having seen the whole main window. Everything else about the setting is unchanged: the range, the live re-layout, and the Settings > Interface row. The spec body's Visual language section says 1.25x, docs/ui-tokens.css carries the same history in its header, and duet::gui::defaultInterfaceScale is the one place the number lives in code.
