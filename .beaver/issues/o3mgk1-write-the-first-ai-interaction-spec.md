---
id: o3mgk1
title: Write the first-AI-interaction spec
state: done
assignee: claude
priority: medium
labels:
    - roadmap:d9gioe
    - session:spec
depends_on:
    - hll1mo
    - sdfjqh
    - lxwoas
    - u64tso
    - u24m3x
parent: d9gioe
created: 2026-08-07T06:03:05Z
updated: 2026-08-08T08:20:36Z
---

Spec session for the AI area: interaction model (hll1mo), backend landscape (sdfjqh), and the chosen first interaction (lxwoas).

Read those nodes' closing notes, interview to close remaining gaps (likely surfaced from the Frontier: the AI seam's concrete API over the project model, real-time safety rules, how suggestions are auditioned/accepted in the UI), confirm, then run /create-specification. Record the AI seam in ARCHITECTURE.md and the settled choices as ADRs.

## Notes

**claude** — 2026-08-08T08:20:36Z

Spec session settled (2026-08-08). The AI-area spec is published as issue js437t ('The Collaborator — milestone-one AI collaboration', label spec), with lineage edges to the five nodes it covers: hll1mo, sdfjqh, lxwoas, u64tso, u24m3x. Implementation slices come from /create-issues js437t — build the sub-issues, never the spec issue.

GAPS CLOSED BY INTERVIEW (all user-decided):
- Spec boundary: the spec owns the AI seam's contracts and behavior; it binds but does not design the shared-undo mechanism (skb4tp), the technical audition mechanism (86t5lu), and conversation/Proposal persistence (rquzdc). Constraint notes were left on skb4tp and 86t5lu.
- Proposal emission: a single 'propose' write-tool whose input schema IS the Proposal; commentary is plain assistant text; at most one propose per Task Run.
- The edit-operation vocabulary is enumerated in the spec, derived from kimula's milestone-one UI surface (MIDI notes, clips, tracks/routing, mixer+sends+sidechain, plugins, drawn automation, tempo/time-signature). No operation creates audio content.
- Run concurrency: one active Task Run at a time; the panel input box is disabled during a run.
- Conversation and Proposals are in-memory per app session in milestone one; persistence arrives with rquzdc.
- Estimate marking is mechanical taint via a per-run estimate ledger (estimate_audio_content results + external-plugin displayString wrappers); never model self-report; over-marking accepted.
- Sidecar mechanism refined: a minimal Node host embedding pi's SDK (the fod077 shape), bundled as a standalone binary, ONE JSON-RPC 2.0 protocol over ONE local socket for prompts, events, cancel, and tool calls. Amendment recorded on lxwoas.
- Model access: no provider treated specially in the picker — the Gemini free-tier warning rider from lxwoas/sdfjqh is dropped (amendment on lxwoas).
- Cherry-pick granularity is the Proposal ELEMENT (one human-meaningful change bundling several operations, independently applicable). Revision replaces a pending Proposal (superseded); replying to a rejected one yields a new pending Proposal.
- Test seams (agreed): the socket protocol (primary, no-LLM harness), the analysis routines as pure functions (worked examples incl. EBU Tech 3341), and the Proposal manager's state machine.

RECORDS: ADR 0002 (deterministic tools, never audio), ADR 0003 (pi-SDK sidecar behind a socket protocol); ARCHITECTURE.md gained the AI seam; GLOSSARY.md gained 'Task Run' and 'Duet Loop'.
