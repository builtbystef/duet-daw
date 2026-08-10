---
id: s11o4w
title: What is the producer-facing UI of milestone one?
state: todo
labels:
    - roadmap:d9gioe
    - session:grill
depends_on:
    - 86t5lu
parent: d9gioe
created: 2026-08-10T03:45:07Z
updated: 2026-08-10T03:45:07Z
---

Grill session for the UI area, split out of the foundation spec at node 86t5lu (2026-08-10): the foundation spec (b1j3me) covers the substrate only, and the producer-facing UI — timeline/arrangement view, piano roll, mixer, transport chrome, browser, and the Collaborator panel's placement — is its own area.

Interview the user to settle: overall layout and navigation between views; which surfaces exist in milestone one and what each shows; interaction conventions (snap, zoom, selection, drag semantics); where the Collaborator panel lives (its behavior is specced at js437t, its placement is not); visual direction at the level a prototype needs. Constraints already binding: JUCE 9 software renderer by default with per-surface OpenGLContext escape hatch (ddp1qt), every edit gesture maps to the vocabulary layer's Actions (b1j3me), milestone-one feature list at kimula.

The answers become the brief for the follow-on prototype node.
