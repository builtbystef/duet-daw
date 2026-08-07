---
id: u64tso
title: What must the Collaborator and Proposal UX look and feel like?
state: todo
priority: high
labels:
    - roadmap:d9gioe
    - session:prototype
depends_on:
    - hll1mo
    - lxwoas
parent: d9gioe
created: 2026-08-07T18:10:08Z
updated: 2026-08-07T18:10:08Z
---

Prototype session. The interaction model is settled (node hll1mo's closing note); the user explicitly deferred the concrete UX for dedicated deliberation — the bar is that the AI interactions feel useful and easy to use mid-flow, not bolted on.

Prototype against the chosen first interaction (node lxwoas), covering the surfaces the model defines:

- The conversation panel: kicking off a task, watching a non-blocking run, canceling it, reading commentary.
- Inline entry points on clips/tracks with the current selection as implicit context.
- Proposal-state in the timeline and mixer: how proposed clips and values look, playing them in context, the A/B toggle for mix changes.
- Accept/reject: the whole-Proposal fast path and per-element cherry-pick.
- The stale marking when the producer's edits touch a pending Proposal, and asking for a redo.

Disposable UI mockups or click-dummies are enough — the deliverable is the settled look and behavior, which feeds the first-AI-interaction spec (node o3mgk1).
