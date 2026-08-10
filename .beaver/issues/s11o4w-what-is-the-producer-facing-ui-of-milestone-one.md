---
id: s11o4w
title: What is the producer-facing UI of milestone one?
state: done
assignee: claude
labels:
    - roadmap:d9gioe
    - session:grill
depends_on:
    - 86t5lu
parent: d9gioe
created: 2026-08-10T03:45:07Z
updated: 2026-08-10T18:32:39Z
---

Grill session for the UI area, split out of the foundation spec at node 86t5lu (2026-08-10): the foundation spec (b1j3me) covers the substrate only, and the producer-facing UI — timeline/arrangement view, piano roll, mixer, transport chrome, browser, and the Collaborator panel's placement — is its own area.

Interview the user to settle: overall layout and navigation between views; which surfaces exist in milestone one and what each shows; interaction conventions (snap, zoom, selection, drag semantics); where the Collaborator panel lives (its behavior is specced at js437t, its placement is not); visual direction at the level a prototype needs. Constraints already binding: JUCE 9 software renderer by default with per-surface OpenGLContext escape hatch (ddp1qt), every edit gesture maps to the vocabulary layer's Actions (b1j3me), milestone-one feature list at kimula.

The answers become the brief for the follow-on prototype node.

## Notes

**claude** — 2026-08-10T18:32:29Z

Grill session settled (2026-08-10). Mid-session the user supplied a Next.js UI mockup (~/Code/temp/duet-daw-ui-mockup, its own git repo) which I ran and explored in the browser; it becomes the visual reference. Colors/theme follow the mockup as closely as practical; layout may change; the Collaborator/Proposal UX keeps the earlier prototype's mechanics (u64tso), not the mockup's simplified card.

LAYOUT — single main window, docked panes; plugin editors are the only floating windows. Timeline/arrangement is the central surface with automation as expandable lanes under tracks. Bottom panel: resizable/collapsible, tabs Piano Roll + Mixer. Left dock: browser (search + favorites; sections: samples from user-chosen folders, the 2 built-in instruments, the 3 built-in effects, scanned VST3 list; drag-to-track/timeline inserts). Right dock: Collaborator panel (u64tso). Transport bar top: bar/beat + wall-time readout, BPM, time-sig, grid-size control, loop/metronome/follow-playhead toggles, CPU% + health indicator, undo/redo, project name. One "Duet" app menu holds New/Open/Save/Save As/Recent, Export/Bounce, Audio & MIDI Settings, Plugin Scan, panel toggles — no File/Edit menu bar. Dialogs: Export/Bounce (name, destination, format, bit depth, sample rate, range, normalize), Audio & MIDI Settings (Audio/MIDI/Interface tabs), plugin-scan flow.

INTERACTION — snap on by default, grid adapts to zoom, visible grid-size control, hold-Alt bypasses mid-drag. Scroll: plain=vertical, Shift=horizontal, Ctrl=h-zoom at pointer, Ctrl+Shift=v-zoom (track/key heights); +/- keys and zoom-to-fit. Selection: click; rubber-band on empty; Ctrl+click toggle; Shift+click extend; Ctrl+A per focused surface; Escape clears; one current selection feeds the Collaborator context chip. Clips: drag=move (snapped), Ctrl+drag=copy, edge=trim, loop-handle extend, double-click MIDI clip opens piano roll, double-click empty MIDI track creates one grid-unit clip, Delete deletes. Single smart tool everywhere — no tool palette. Keys (mockup-verified set): Space play/stop, R record, L loop, M metronome, Home/End, B browser, C collaborator, E bottom panel, P piano-roll tab, X mixer tab, F follow, Ctrl+Z/Ctrl+Shift+Z, Ctrl+S.

VISUAL — the mockup's "Graphite" theme is the direction: achromatic chrome (contrast carries emphasis; accent near-white on dark / near-black on light), BOTH dark and light modes (OS preference on first launch, switchable in Settings > Interface) — this REVERSES the earlier in-session dark-only lean at the user's explicit decision. 8 desaturated user-assignable track colors, semantic info/success/warning/danger, Inter (OFL, bundled) with tabular numerals for time displays, mid-density flat surfaces, thin scrollbars. Token set and both palettes: app/globals.css in the mockup repo.

AI ACCENT — TEAL, reserved exclusively for the Collaborator (✦ badges, proposal glow/ghosts, ghost fader handles, commentary accents): approx #3fd0be on dark / #0e7c70 on light, tuned at the prototype. Deliberately brighter than the muted track-cyan/mint so it stands apart; avoids all four semantic hues (stale keeps amber). Violet/purple was explicitly rejected as AI-cliche.

COLLABORATOR/PROPOSAL UX — u64tso mechanics in full (per-element cherry-pick, stale-amber + redo-against-current, rejection-with-reason revision, ghost+glow clips, ghost fader + A/B mix toggle) wearing the mockup's panel/card styling; the audition button is labeled "Audition" per the glossary (mockup said "Preview"); History section (applied proposals) kept, in-memory per session per js437t.

SCOPE ADDITIONS (milestone one) — piano roll gains scale highlighting + Fold and a note-length control; browser gains search + favorites; Settings > Interface gains global interface scaling; mixer strips edit I/O routing and insert chains in place. SCOPE HELD — built-ins stay 2 instruments + 3 effects (kimula); the mockup's third instrument ("Duet Drum Rack") and six effects are set dressing. AUTOSAVE — becomes a setting (off/2/5/10 min) with DEFAULT 10 MIN, amending b1j3me's fixed 5-minute autosave; recovery-file mechanism unchanged.

Feeds prototype node r4m858, whose brief is this note plus the mockup repo.
