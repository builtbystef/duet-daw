---
id: hll1mo
title: What is the AI collaborator's interaction model?
state: done
assignee: claude
priority: high
labels:
    - roadmap:d9gioe
    - session:grill
depends_on:
    - kimula
parent: d9gioe
created: 2026-08-07T06:02:05Z
updated: 2026-08-07T18:09:55Z
---

Grill session on the product's USP, limited to the interaction model — not backends, not implementation:

- What the AI can SEE: the whole session? the arrangement, the mix state, the audio itself?
- What the AI can DO: suggest, or also act? Generate MIDI/audio, edit arrangement, adjust the mix, give feedback? With what autonomy, and what does the producer approve?
- How the producer INVOKES it: chat panel, inline actions on clips/tracks, an always-on collaborator that offers unprompted?
- The duet framing: turn-taking, or continuous presence? How does a suggestion get auditioned, accepted, rejected?

The answer defines the AI seam in ARCHITECTURE.md conceptually, and gates the AI-backend research. "Tasteful and actually useful" is the bar — the session should stress-test each capability against that.

## Notes

**claude** — 2026-08-07T18:09:55Z

Grill session settled (2026-08-07). The interaction model:

INVOCATION — producer-initiated only. The Target Producer kicks off a task; an agent/workflow runs to complete it. Runs are non-blocking (the producer keeps playing and editing) and cancelable. No ambient/unprompted observation — moved to Out of scope on the root. The only unprompted signal that exists is "your task finished."

SIGHT — the Collaborator sees the entire project state (arrangement, clips, MIDI notes, mixer, automation, plugin names and parameters) with no manual context curation, and consumes rendered audio (a clip, a track, the mix) on demand — never continuously.

OUTPUT — a task's result is commentary, a Proposal, or both (e.g. mix feedback with the EQ change attached). A Proposal is a bundle of discrete project changes that alters nothing until accepted; no autonomous mutation, ever. Accept/reject works at both levels: whole-Proposal fast path, and per-element cherry-pick.

EDIT VOCABULARY — closure principle: a Proposal may contain exactly what the Target Producer could do through the UI (MIDI, clips, arrangement moves, mixer settings, automation, plugin parameters). Nothing more (no hidden state, no project-file surgery), nothing less (no format carve-outs — usefulness decides what ships first, not the model). One edit vocabulary shared by human and AI; this also feeds the shared undo/redo design on the Frontier.

SURFACES — a docked conversation panel is the primary surface (requests, run progress, results); inline entry points on clips/tracks are shortcuts into the same Collaborator, with the current selection as implicit context. The detailed UX is explicitly deferred: the user wants dedicated deliberation so the interactions feel useful and easy — now a prototype node blocked on the first-interaction choice (lxwoas).

AUDITION — Proposals materialize in place: proposed clips and values appear in the timeline and mixer, visually marked as proposal-state, playable in context with the rest of the arrangement. Mix-change Proposals get an A/B toggle during playback.

DUET LOOP — the panel holds a running within-session conversation; replying to a pending or rejected Proposal yields a revised Proposal, and rejection-with-a-reason is first-class input. Multiple pending Proposals may coexist, independently. A Proposal whose target the producer has since edited is marked stale — still auditionable, never auto-merged; the producer can ask for a redo against current state. The producer's live edits always win. What carries across sessions stays on the Frontier.

TERMS — "Collaborator" and "Proposal" recorded in docs/GLOSSARY.md.

WHY — user-initiated task runs match how the user wants AI to work and avoid the Clippy failure; Proposals preserve trust and authorship in a creative tool; full sight plus audio-on-demand is what makes feedback real; the closure principle keeps the AI seam honest and one undo model possible; in-place, in-context audition is the product thesis — a duet, not a chatbot with an export button.
