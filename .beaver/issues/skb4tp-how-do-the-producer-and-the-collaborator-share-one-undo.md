---
id: skb4tp
title: How do the producer and the Collaborator share one undo history over Tracktion Engine's Edit?
state: todo
priority: high
labels:
    - roadmap:d9gioe
    - session:prototype
depends_on:
    - lf8tnt
    - psmj4y
parent: d9gioe
created: 2026-08-08T01:29:11Z
updated: 2026-08-09T20:26:13Z
---

Prototype session (disposable code, headless — no GUI needed). Made sharp by node lf8tnt, which adopted Tracktion Engine, and constrained by node hll1mo, which settled that one edit vocabulary is shared by the producer and the Collaborator and that every Collaborator change enters the project only as a Proposal the producer accepts.

The question: what does Duet's edit vocabulary look like as a layer over `tracktion::engine::Edit`, such that a human edit and an accepted Proposal are indistinguishable to undo, and one undo step means one thing a producer would recognise?

Three findings from lf8tnt make this urgent rather than theoretical:

1. **Transaction boundaries are decided by a 350 ms timer tied to mouse-up.** `Edit::UndoTransactionTimer` calls `beginNewTransaction()` 350 ms after the last change once the mouse is up (`tracktion_Edit.cpp:20-58`). A Collaborator applying a multi-step edit programmatically has no mouse, so without explicit `beginNewTransaction()` calls its edits will either merge into whatever the producer was just doing or fragment arbitrarily. The engine's own tests call `beginNewTransaction()` explicitly; Duet must too, and the layer is where that decision belongs.
2. **Undo is opt-in per call, not automatic.** Every mutating API takes a `juce::UndoManager*` and callers may legally pass `nullptr` — `MidiList::addNote`, `AutomationCurve::addPoint`, `TempoSequence::insertTempo` all have this shape. At least one engine path already passes null: live plugin-parameter setting is not undoable (`tracktion_AutomatableParameter.cpp:888`). A vocabulary layer that forgets the manager on one operation produces an edit that silently cannot be undone.
3. **Undo automatically rebuilds the audio graph**, because undo is itself a ValueTree mutation and `Edit::TreeWatcher` triggers `restartPlayback()` on tree changes. This is the property that makes the whole design tractable — verify it holds for the operations Duet actually exposes, including while the transport is rolling.

What the spike must demonstrate, headlessly:
- A small set of Duet edit operations (insert clip, move clip, trim clip, add MIDI note, set an automation point) expressed once, callable from both a simulated producer action and a simulated accepted Proposal, with identical undo behaviour.
- A multi-step Proposal collapsing to exactly one undo step, and undoing cleanly in one action.
- Undo and redo while the transport is rolling, without a glitch or a stale graph.
- What happens to the undo stack when a Proposal is rejected — the design should make rejection cost nothing, so confirm nothing entered the history.

Also settle the seam question this exposes: how much of `tracktion::engine` is allowed to appear in Duet's own types. Node lf8tnt's recommendation rests on keeping the engine behind Duet's vocabulary so a future engine swap is not a rewrite; this session is where that boundary gets drawn concretely rather than aspirationally.

Deliverable: the shape of the edit-vocabulary layer and the undo/transaction rules, with the spike's findings — ready to become an ADR and to feed both the foundation spec (86t5lu) and the AI-area spec.

## Notes

**claude** — 2026-08-08T08:20:18Z

Constraint from the AI-area spec (js437t, 2026-08-08): accepting a Proposal — or a single cherry-picked element — must land in the shared undo history as ONE undoable action. The spec binds only this observable behavior; the mechanism is this node's to design.

**claude** — 2026-08-09T20:26:13Z

Two findings from ddp1qt (2026-08-09) that land directly on this node's design:
1. Every programmatic clip insertion must pin its source reference: AudioTrack::insertWaveClip stores a path relative to the edit's TEMP directory ('../x.wav'), which silently plays silence. Fix proven in the skeleton: SourceFileReference::setToFile(file, PathStyle::alwaysAbsolute, false) — but the edit-vocabulary layer should decide the reference policy once, centrally, not per call site.
2. Do not use EditFileOperations::save for project-less edits: it segfaults in EditSnapshot::refresh() (tracktion_EditSnapshot.cpp:227 null-derefs the ProjectItem; upstream bug, reproduced 3x, gdb-confirmed). The skeleton's working save path is edit->flushState() + write edit->state XML directly. Duet's save mechanism must go through the vocabulary layer's own path.
Also proven here and relevant: 600+ structural mutations during playback produced zero xruns — the TreeWatcher rebuild this node's undo design leans on is confirmed inaudible.
