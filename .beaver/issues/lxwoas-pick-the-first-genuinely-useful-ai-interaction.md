---
id: lxwoas
title: Pick the first genuinely useful AI interaction
state: done
assignee: claude
priority: high
labels:
    - roadmap:d9gioe
    - session:grill
depends_on:
    - hll1mo
    - sdfjqh
    - fod077
parent: d9gioe
created: 2026-08-07T06:02:50Z
updated: 2026-08-08T03:35:16Z
---

Grill session. With the interaction model decided (hll1mo) and the backend landscape known (sdfjqh), pick the ONE AI interaction that ships in milestone one — the feature that proves "duet" is real, not a gimmick.

Candidates will come from the earlier nodes; the session weighs each for: usefulness to the target producer on day one, feasibility with the chosen backends, and whether it exercises the AI seam the architecture will need anyway. Also decide the fallback behavior when the backend is unreachable.

The answer completes the AI area; its spec node follows.

## Notes

**claude** — 2026-08-08T03:35:11Z

Grill session settled (2026-08-08). The first AI interaction is chosen — and it is the full shape, not a domain slice.

THE INTERACTION — milestone one ships the Collaborator whole: the producer asks anything about the project in the conversation panel (or via inline entry points); the Collaborator interrogates the project through deterministic analysis tools; it returns commentary, a Proposal, or both, in ANY edit domain the closure principle covers (MIDI, clips, arrangement, mixer, automation, plugin parameters). The domain-sliced candidates (Mix Doctor / Arrangement Doctor / Harmony Helper) were considered and rejected in favor of the full shape. Consequence accepted knowingly: proposal-state audition UX for every edit type is milestone-one work, and so is the full analysis tool layer (including the measured-loudness tooling lf8tnt flagged as absent from Tracktion).

DUET LOOP SCOPE — the full hll1mo mechanics ship in milestone one: multiple independent pending Proposals, stale-marking when the producer edits a Proposal's target, per-element cherry-pick, revise-on-reply with rejection-as-input, in-place audition, A/B toggle for mix changes. No trimmed first pass.

BACKEND — the agent loop is pi (MIT), run as a sidecar process, BUNDLED into the DAW install as a standalone binary, invisible to the user. This settles the Frontier's native-C++-vs-sidecar question. Tool calls reach the DAW through the seam hcxgfv documented: pi's RPC mode cannot register host tools, so a thin TypeScript extension shim (pi.registerTool()) forwards each tool call to the DAW over a local socket. The escape hatch is real and stays open — the tool contracts transfer unchanged to a native C++ loop if the sidecar disappoints. The sidecar's footprint (disk, startup latency) is unmeasured; measure it in the walking skeleton (ddp1qt). Rationale for accepting the Node runtime: the AI collaboration is not performance-constrained (runs are non-blocking per hll1mo), and pi buys the multi-provider abstraction, session tree (fork() maps onto Proposals), and auto-compaction that would otherwise be hand-written C++.

MODEL ACCESS — bring-your-own credentials: users log into model-provider subscriptions via pi or enter an API key. A model picker exposes whatever pi providers the user configured. Recommended default: gpt-5.6-terra (7/7 in the fod077 run; Grok 4.5 went 6/7 with one over-critique). Riders from sdfjqh: Gemini FREE-TIER keys are refused or loudly warned against (Google trains on unpaid-tier content with human reviewers); with no credentials configured, the panel shows a setup state, not an error.

FALLBACK — when the backend is unreachable (LLM API down, no network, or the sidecar process dead — same handling), the task FAILS FAST: immediate, clearly visible "Collaborator is offline" state in the panel; nothing queues, no retry loop, and the DAW is otherwise fully functional. No local-model fallback: the target producer's hardware (e.g. a MacBook) cannot run a useful local model, and a silent quality drop would be worse than an honest failure. Local inference moves to Out of scope for milestone one.

WHY THE FULL SHAPE — the fod077 prototype validated the whole loop across all domains with one five-tool draft vocabulary (7/7 and 6/7 across mix, arrangement, and harmony fixtures); slicing by domain would have saved audition UX but shipped a Collaborator that says "I can't help with that" inside its own core competence, undercutting the duet thesis. Usefulness decides what ships first, and the prototype showed the whole shape is useful.

This closes the AI area's last decision. The spec node (o3mgk1) follows; the tool vocabulary (which analyses, what each returns, sufficiency principle) is now sharp and becomes its own node.
