---
id: 53vwbd
title: Write the UI-area spec
state: done
assignee: claude
labels:
    - roadmap:d9gioe
    - session:spec
depends_on:
    - s11o4w
    - r4m858
parent: d9gioe
created: 2026-08-10T19:04:40Z
updated: 2026-08-10T22:39:59Z
---

The UI area's decisions are all closed: layout, surfaces, interaction conventions, visual direction and scope at grill s11o4w; the concrete look-and-behave verdicts (final teal values, dark-text amendment, 1.25x default interface scale, proposal rendering, grid/zoom feel, smart tool, docking) at prototype r4m858 (branch prototype/milestone-ui). The standing visual reference is the user's mockup at ~/Code/temp/duet-daw-ui-mockup with the r4m858 amendments layered on top.

Read the closing notes of s11o4w and r4m858 (only this area's nodes), interview to close any remaining gaps, confirm with the user, then run /create-specification. Publish the spec issue with blocking edges back to s11o4w and r4m858.

## Notes

**claude** — 2026-08-10T22:39:59Z

Spec session complete (2026-08-10). The UI-area spec is published as issue 535bbo ("The Interface — producer-facing UI for milestone one", label spec), with blocking edges back to s11o4w and r4m858. Gaps closed by interview here, all now recorded in the spec: UI-state persistence split (app-global in PropertyStorage; per-project view state in a VIEW child of the DUET tree, nullptr-UM writes, captured at save time); launch/close behavior (reopen last project, no start screen, Save/Discard/Cancel prompt, new project seeds one synth track + one audio track); track lifecycle (+ Add Track row, drag-reorder, header context menu); minimal context menus mirroring vocabulary Actions (note menu includes Quantize, confirmed in kimula's scope); automation linear-only in milestone one with the data model staying curve-ready; untitled projects are real folders from creation in a configurable default projects directory (~/Music/Duet Projects); Save As copies the whole project folder. Test seams agreed: the existing Action seam (ADR 0004) plus a new paintless view-model seam inside the new duet_gui module. Implementation slices come from /create-issues 535bbo — build the sub-issues, never the spec issue.
