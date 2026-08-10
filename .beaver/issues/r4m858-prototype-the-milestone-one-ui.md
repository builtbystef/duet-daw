---
id: r4m858
title: Prototype the milestone-one UI
state: todo
labels:
    - roadmap:d9gioe
    - session:prototype
depends_on:
    - s11o4w
parent: d9gioe
created: 2026-08-10T03:45:15Z
updated: 2026-08-10T18:33:33Z
---

Prototype session for the UI area, following the grill session s11o4w — the brief is s11o4w's closing note plus the user's Next.js mockup at ~/Code/temp/duet-daw-ui-mockup (the standing visual reference: "Graphite" achromatic theme, dark + light palettes in app/globals.css, layout and interaction conventions verified there).

Build a disposable JUCE UI prototype the user can drive live: the arrangement timeline, piano roll, and mixer at whatever fidelity answers the look-and-behave questions the grill left concrete — among them: tuning the reserved TEAL Collaborator accent (approx #3fd0be dark / #0e7c70 light) against the Graphite palette in both modes, the ghost+glow proposal rendering (u64tso) recolored to teal, adaptive grid feel under Ctrl-scroll zoom, smart-tool clip/note editing, and the bottom-panel/browser/collaborator docking behavior. Reuse the walking-skeleton findings (ddp1qt: software renderer passed at 60-track density; GL escape hatch proven) rather than re-measuring. The verdicts feed the UI-area spec.
