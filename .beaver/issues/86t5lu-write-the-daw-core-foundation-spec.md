---
id: 86t5lu
title: Write the DAW-core foundation spec
state: todo
priority: medium
labels:
    - roadmap:d9gioe
    - session:spec
depends_on:
    - kimula
    - 1hn16k
    - lf8tnt
    - psmj4y
    - ddp1qt
    - hvv3nn
    - skb4tp
    - rquzdc
parent: d9gioe
created: 2026-08-07T06:03:05Z
updated: 2026-08-08T08:20:18Z
---

Spec session for the foundation area: milestone scope (kimula), application foundation (1hn16k), engine layer (lf8tnt), toolchain (psmj4y), and the walking-skeleton findings (ddp1qt).

Read those nodes' closing notes, interview to close remaining gaps (likely surfaced from the Frontier: project data model, persistence, undo/redo, thread model), confirm, then run /create-specification. Record the settled choices as ADRs, and the toolchain commands in AGENTS.md (closes l1gtax).

## Notes

**claude** — 2026-08-08T08:20:18Z

Constraints from the AI-area spec (js437t, 2026-08-08), which binds behavior whose mechanisms are foundation-area work: (1) proposal-state audition — ghost clips and ghost mixer values playable in context, A/B toggle for mix changes — needs a mechanism for entering the playback graph without entering the project state; (2) the Collaborator service's threading assumptions (socket on its own thread, project-model reads marshalled to the message thread, analysis on worker threads, nothing on the audio thread) must fit the thread model this spec settles; (3) tier-2 analysis needs a per-track offline render path through the engine.
