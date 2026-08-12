---
id: skb4tp
title: How do the producer and the Collaborator share one undo history over Tracktion Engine's Edit?
state: done
assignee: claude
priority: high
labels:
    - roadmap:d9gioe
    - session:prototype
depends_on:
    - lf8tnt
    - psmj4y
parent: d9gioe
created: 2026-08-08T01:29:11Z
updated: 2026-08-09T22:44:43Z
---

Prototype session (disposable code, headless — no GUI needed). Made sharp by node lf8tnt, which adopted Tracktion Engine, and constrained by node hll1mo, which settled that one edit vocabulary is shared by the producer and the Collaborator and that every Collaborator change enters the project only as a Suggestion the producer accepts.

The question: what does Duet's edit vocabulary look like as a layer over `tracktion::engine::Edit`, such that a human edit and an accepted Suggestion are indistinguishable to undo, and one undo step means one thing a producer would recognise?

Three findings from lf8tnt make this urgent rather than theoretical:

1. **Transaction boundaries are decided by a 350 ms timer tied to mouse-up.** `Edit::UndoTransactionTimer` calls `beginNewTransaction()` 350 ms after the last change once the mouse is up (`tracktion_Edit.cpp:20-58`). A Collaborator applying a multi-step edit programmatically has no mouse, so without explicit `beginNewTransaction()` calls its edits will either merge into whatever the producer was just doing or fragment arbitrarily. The engine's own tests call `beginNewTransaction()` explicitly; Duet must too, and the layer is where that decision belongs.
2. **Undo is opt-in per call, not automatic.** Every mutating API takes a `juce::UndoManager*` and callers may legally pass `nullptr` — `MidiList::addNote`, `AutomationCurve::addPoint`, `TempoSequence::insertTempo` all have this shape. At least one engine path already passes null: live plugin-parameter setting is not undoable (`tracktion_AutomatableParameter.cpp:888`). A vocabulary layer that forgets the manager on one operation produces an edit that silently cannot be undone.
3. **Undo automatically rebuilds the audio graph**, because undo is itself a ValueTree mutation and `Edit::TreeWatcher` triggers `restartPlayback()` on tree changes. This is the property that makes the whole design tractable — verify it holds for the operations Duet actually exposes, including while the transport is rolling.

What the spike must demonstrate, headlessly:
- A small set of Duet edit operations (insert clip, move clip, trim clip, add MIDI note, set an automation point) expressed once, callable from both a simulated producer action and a simulated accepted Suggestion, with identical undo behaviour.
- A multi-step Suggestion collapsing to exactly one undo step, and undoing cleanly in one action.
- Undo and redo while the transport is rolling, without a glitch or a stale graph.
- What happens to the undo stack when a Suggestion is rejected — the design should make rejection cost nothing, so confirm nothing entered the history.

Also settle the seam question this exposes: how much of `tracktion::engine` is allowed to appear in Duet's own types. Node lf8tnt's recommendation rests on keeping the engine behind Duet's vocabulary so a future engine swap is not a rewrite; this session is where that boundary gets drawn concretely rather than aspirationally.

Deliverable: the shape of the edit-vocabulary layer and the undo/transaction rules, with the spike's findings — ready to become an ADR and to feed both the foundation spec (86t5lu) and the AI-area spec.

## Notes

**claude** — 2026-08-08T08:20:18Z

Constraint from the AI-area spec (js437t, 2026-08-08): accepting a Suggestion — or a single cherry-picked element — must land in the shared undo history as ONE undoable action. The spec binds only this observable behavior; the mechanism is this node's to design.

**claude** — 2026-08-09T20:26:13Z

Two findings from ddp1qt (2026-08-09) that land directly on this node's design:
1. Every programmatic clip insertion must pin its source reference: AudioTrack::insertWaveClip stores a path relative to the edit's TEMP directory ('../x.wav'), which silently plays silence. Fix proven in the skeleton: SourceFileReference::setToFile(file, PathStyle::alwaysAbsolute, false) — but the edit-vocabulary layer should decide the reference policy once, centrally, not per call site.
2. Do not use EditFileOperations::save for project-less edits: it segfaults in EditSnapshot::refresh() (tracktion_EditSnapshot.cpp:227 null-derefs the ProjectItem; upstream bug, reproduced 3x, gdb-confirmed). The skeleton's working save path is edit->flushState() + write edit->state XML directly. Duet's save mechanism must go through the vocabulary layer's own path.
Also proven here and relevant: 600+ structural mutations during playback produced zero xruns — the TreeWatcher rebuild this node's undo design leans on is confirmed inaudible.

**claude** — 2026-08-09T22:44:34Z

ANSWERED 2026-08-09 (prototype session; branch prototype/undo-vocabulary, dir prototype-undo-vocabulary/). Verdict from the user, after driving the interactive GUI: 'Tested it out, everything seems to work properly. Good with me.' 28/28 scripted assertions pass against pinned JUCE 9 + Tracktion develop.

THE DESIGN — Duet's edit vocabulary as a layer over te::Edit:
1. One class (spike: duet::ProjectEditor, DuetEdit.h) whose interface exposes ZERO tracktion/juce types: opaque uint64 TrackRef/ClipRef wrapping EditItemID::getRawID/fromRawID, time as seconds/beats doubles. The scenario runner and the GUI compiled against it without a single engine type — that is the engine seam, drawn concretely. Only DuetEdit.cpp includes tracktion_engine.h.
2. Each edit op is expressed exactly once (insertAudioClip, moveClip, trimClip, addMidiNote, setAutomationPoint) and ALWAYS passes the Edit's UndoManager — never nullptr. insertAudioClip pins SourceFileReference alwaysAbsolute centrally (ddp1qt rule made policy).
3. Actions are the ONLY transaction boundary: performAction(name, ops) = beginNewTransaction(name) + ops, synchronous on the message thread. NO closing beginNewTransaction — deliberate, so the engine's deferred tracked writes (finding 2 below) merge into the action that caused them. A producer gesture and acceptSuggestion (all ops of a Suggestion) both go through performAction => undo parity and one-undo-step collapse hold BY CONSTRUCTION (proven: S1 parity, S2 5-op collapse to one named step, undo+redo digests identical).
4. A Suggestion is DATA (an op list with a newest-clip placeholder for intra-suggestion references) until accepted; rejection discards the data => zero trace in project, undo and redo stacks (S3). Element cherry-pick = accepting a sub-list.
5. Undo/redo route through edit->undo()/redo() (they stop recording first; transport properties are um=nullptr so undo can never stop/reposition the transport).
6. Rolling transport: accept/undo/redo mid-playback works — clip+automation toggle audibly, loop wraps, transport survives, ZERO xruns across all cycles (S5 + user's GUI session).

ENGINE FINDINGS the implementation and specs must honor:
F1. Ops outside an action merge into the previous action's transaction, or land in an UNNAMED step after the engine's 350ms UndoTransactionTimer seals (timer fires when no mouse is down; message-loop quiet 350ms). Ops must therefore be private to the action mechanism. Edit::UndoTransactionInhibitor (RAII) exists to suspend the timer for long actions.
F2. After any clip move/trim the engine schedules an ASYNC UNDO-TRACKED clip re-sort (ClipOwner.cpp:125, ValueTree::sort with the Edit um) that lands in whatever transaction is open at the next message pump — and CLEARS THE REDO STACK if it fires after an undo. Mitigations: actions stay open (no seal) so the sort joins its cause; suppressed during undo/redo by the engine itself.
F3. Edit::flushState() WRITES WITH THE UNDO MANAGER (AutomatableEditItem::saveChangedParametersToState, tracktion_AutomatableEditItem.cpp:321): saving pollutes undo history with unnamed transactions and kills redo after an undo. Duet's save path (already custom per ddp1qt: flushState + direct XML write) must account for this — e.g. save at action boundaries and treat the flush transaction deliberately. Feeds rquzdc.
F4. Undo/redo round trips PERMUTE ValueTree property order (juce undo re-appends restored properties). Any state digest/comparison must canonicalize (sort properties). Matters for tests.
F5. Red herring resolved: transport dying ~3s after first playback is the DeviceManager's ONE-TIME async wave-device-list rebuild (handleAsyncUpdate -> releaseDeviceList -> clearNodes -> playhead stops -> transport timer stops transport), proven by a no-mutation control run. Headless/console contexts hit it; a GUI app finishes it before the user plays. Not undo-related. Also: the first play() after construction is ignored until the message loop runs (async context allocation) — retry play until isPlaying.
F6. Local build caveat: full-parallel builds of this stack OOM-freeze a 15GB/12-core machine (~2GB per Tracktion TU); build with -j 4.

Ready to become an ADR; feeds 86t5lu (foundation spec) and the AI-area spec js437t (accept-as-one-step mechanism is now proven).
