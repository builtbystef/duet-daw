---
id: r4m858
title: Prototype the milestone-one UI
state: done
assignee: claude
labels:
    - roadmap:d9gioe
    - session:prototype
depends_on:
    - s11o4w
parent: d9gioe
created: 2026-08-10T03:45:15Z
updated: 2026-08-10T19:04:27Z
---

Prototype session for the UI area, following the grill session s11o4w — the brief is s11o4w's closing note plus the user's Next.js mockup at ~/Code/temp/duet-daw-ui-mockup (the standing visual reference: "Graphite" achromatic theme, dark + light palettes in app/globals.css, layout and interaction conventions verified there).

Build a disposable JUCE UI prototype the user can drive live: the arrangement timeline, piano roll, and mixer at whatever fidelity answers the look-and-behave questions the grill left concrete — among them: tuning the reserved TEAL Collaborator accent (approx #3fd0be dark / #0e7c70 light) against the Graphite palette in both modes, the ghost+glow suggestion rendering (u64tso) recolored to teal, adaptive grid feel under Ctrl-scroll zoom, smart-tool clip/note editing, and the bottom-panel/browser/collaborator docking behavior. Reuse the walking-skeleton findings (ddp1qt: software renderer passed at 60-track density; GL escape hatch proven) rather than re-measuring. The verdicts feed the UI-area spec.

## Notes

**claude** — 2026-08-10T19:04:27Z

Verdict (2026-08-10, user: "This is all great now") after two live iterations of a GUI-only JUCE prototype — branch prototype/milestone-ui, directory prototype-ui/ (arrangement + piano roll + mixer + browser + Collaborator panel + transport bar, seeded in-memory demo project, live theme toggle and teal HSB tuner).

SETTLED VALUES:
- TEAL (Collaborator accent): dark mode #4aa294 — the grill's #3fd0be was explicitly rejected by the user as neon/"Tron"/corny on dark surfaces; light mode #0e7c70 confirmed unchanged. Amends s11o4w's approximations.
- GRAPHITE PALETTES: light mode confirmed exactly as the mockup's app/globals.css; dark mode TEXT brightened one step for readability (primary #b8b8b8→#d2d2d2, secondary #888888→#9e9e9e, muted #6e6e6e→#828282, disabled #4d4d4d→#5c5c5c). Dark surfaces/borders/track colors unchanged.
- SIZING: the mockup's pixel sizes at 1:1 were judged too small to read comfortably; a 1.25x global interface scale was judged right. Milestone one defaults Settings > Interface scaling to 1.25x equivalent (user-adjustable per s11o4w).
- SUGGESTION RENDERING: approved as prototyped — ghost clips fill teal ~12% alpha with dashed teal border + 3-ring soft glow, intensifying on Audition (~26% fill, solid border); teal ✦ prefix on ghost clip names; mixer ghost fader as translucent teal handle with glow, plus per-strip "A: CURRENT / B: PROPOSED" chip visible only while auditioning; suggestion card with teal glow border, per-element cherry-pick checkboxes, Audition/Accept/Reject; excluded elements render at ~35% intensity.
- GRID/ZOOM FEEL: approved — adaptive grid picks the finest subdivision with >=18 px spacing (bar/beat/fine line weights from the token set), Ctrl-scroll h-zoom anchored at pointer, Ctrl+Shift v-zoom, Shift-scroll pan, +/-/0 keys.
- SMART TOOL: approved — click/rubber-band/Ctrl-toggle selection, snapped drag-move with Alt bypass, Ctrl-drag copy, edge trim, Delete; double-click empty MIDI lane creates a one-grid-unit clip, double-click MIDI clip opens piano roll; piano roll with scale highlighting, Fold, note-length chip, double-click note add/remove.
- DOCKING: approved — B/C/E panel toggles, P/X bottom tabs, draggable dividers between all three docks and the arrangement.

Prototype-level bugs (not design findings): JUCE artefact path is build/duet_ui_prototype_artefacts/, and same-bounds setBounds skips resized() so tab visibility must be refreshed explicitly.

Feeds the UI-area spec.
