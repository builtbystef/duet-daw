---
id: u64tso
title: What must the Collaborator and Suggestion UX look and feel like?
state: done
assignee: claude
priority: high
labels:
    - roadmap:d9gioe
    - session:prototype
depends_on:
    - hll1mo
    - lxwoas
parent: d9gioe
created: 2026-08-07T18:10:08Z
updated: 2026-08-08T05:13:53Z
---

Prototype session. The interaction model is settled (node hll1mo's closing note); the user explicitly deferred the concrete UX for dedicated deliberation — the bar is that the AI interactions feel useful and easy to use mid-flow, not bolted on.

Prototype against the chosen first interaction (node lxwoas), covering the surfaces the model defines:

- The conversation panel: kicking off a task, watching a non-blocking run, canceling it, reading commentary.
- Inline entry points on clips/tracks with the current selection as implicit context.
- Suggestion-state in the timeline and mixer: how suggested clips and values look, playing them in context, the A/B toggle for mix changes.
- Accept/reject: the whole-Suggestion fast path and per-element cherry-pick.
- The stale marking when the producer's edits touch a pending Suggestion, and asking for a redo.

Disposable UI mockups or click-dummies are enough — the deliverable is the settled look and behavior, which feeds the first-AI-interaction spec (node o3mgk1).

## Notes

**claude** — 2026-08-08T05:13:53Z

Prototype session settled (2026-08-08). The Collaborator/Suggestion UX is decided; verdict stated by the user against an interactive click-dummy (branch prototype/collaborator-ux, prototypes/collaborator-ux.PROTOTYPE.html — self-contained HTML, open in a browser).

CONVERSATION PANEL — docked right. Holds the running transcript: producer messages (with the current selection shown as a small context chip on the message), Collaborator commentary in accent-colored bubbles, and system lines. Quick-prompt chips above the input adapt to the current selection. A task run is a card in the transcript: spinner, a "you can keep editing" hint, a Cancel button, and rotating friendly status phrases ("putting headphones on…", "counting bars…") that animate in. RAW TOOL CALLS ARE NEVER SHOWN TO END USERS — they are development-mode information only; end users get the rotating phrases. Cancel leaves a "task canceled, nothing changed" line.

INLINE ENTRY POINTS — via the right-click context menu, not floating affordances. Clicking a clip only selects it; selection-triggered "ask AI" chips were rejected as invasive. "✦ Ask Collaborator" sits in the ordinary context menu of a clip or track, among Cut/Copy/Duplicate/Export etc.; choosing it makes the clicked thing the implicit context and focuses the panel composer.

SUGGESTION-STATE IN THE TIMELINE — suggested clips materialize in place, rendered with BOTH reduced opacity (~60%) AND a glowing accent ring, plus a small ✦ AI badge. Ghost alone reads as a muted track; glow alone at full opacity does not read as impermanent; the combination is deliberate and settled. Suggested clips are auditionable in context but not draggable.

SUGGESTION-STATE IN THE MIXER — approved as prototyped: a ghost fader handle at the suggested value next to the real handle, and plugin-parameter changes as a ✦-marked line under the strip. Mix Suggestions carry an A/B toggle (A current / B suggested) on the card that swaps the heard values during playback.

ACCEPT/REJECT — approved as prototyped: per-element ✓/✗ (cherry-pick) on each Suggestion-card row, plus whole-Suggestion "Accept all" and "Reject". Accepting an element materializes it (clip turns permanent, value commits); rejecting removes it. Rejecting invites a typed reason, which yields a revision (revise-on-reply per hll1mo).

STALE — approved as prototyped: a producer edit touching a pending Suggestion's target flips the card and all its ghost-marks to amber STALE; still auditionable, never auto-merged; the card gains "↻ Redo against current state", which resolves the stale Suggestion and starts a fresh run.

BACKEND FAILURE — simplified from lxwoas's "visible offline state": NO dedicated offline banner or persistent offline UI. A failed task drops a plain error line in the conversation ("The Collaborator isn't working right now — try again later"). Still fail-fast, nothing queues, the DAW keeps working — only the presentation is a simple transient error, like any AI app.

Feeds the AI-area spec (o3mgk1).
