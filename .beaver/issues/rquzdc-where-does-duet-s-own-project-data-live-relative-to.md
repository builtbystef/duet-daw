---
id: rquzdc
title: Where does Duet's own project data live relative to Tracktion's Edit file, and how does it version?
state: todo
priority: medium
labels:
    - roadmap:d9gioe
    - session:prototype
depends_on:
    - lf8tnt
    - ddp1qt
parent: d9gioe
created: 2026-08-08T01:29:32Z
updated: 2026-08-09T22:44:43Z
---

Prototype session (disposable code). Made sharp by node lf8tnt: adopting Tracktion Engine settles the *engine's* persistence but not Duet's.

What lf8tnt established:
- A saved Edit is the `Edit`'s `juce::ValueTree` written as XML — `edit.flushState(); if (auto xml = edit.state.createXml()) ok = xml->writeTo (file);` (`tracktion_EditFileOperations.cpp:172-202`). A binary ValueTree variant exists for temp/autosave; loading accepts either, XML first. Extensions are `*.tracktionedit;*.trkedit`.
- The engine's own example app stores application-defined state as a child tree under `edit.state` (`EDITVIEWSTATE`, an identifier the *example* declares, not the engine), with properties bound through the Edit's UndoManager — `examples/common/Components.h:14,43-60`. It persists because saving serialises `edit.state` wholesale.
- There is a first-party hook acknowledging that applications keep their own trees referencing engine object IDs: `applyNewIDsToExternalValueTree` (`model/edit/tracktion_EditItem.h:70-77`).

What is still open, and why it needs a session:

1. **Where Duet's data goes.** Duet will have state the engine has no concept of: the Collaborator's conversation and cross-session memory, Proposal history, per-track or per-clip annotations the AI tools read. Options are a child tree under `edit.state` (persists automatically, undoable, but couples Duet's data to the engine's file), a sidecar file next to the Edit (clean separation, but two files to keep in sync and no free undo), or a container format holding both. The AI data strategy makes this load-bearing: the project data model is the entire AI surface, so where Duet's own facts live decides what the tools can read.

2. **Whether custom properties on engine-owned nodes survive a round trip.** UNRESOLVED in lf8tnt and empirically testable in an afternoon. Save and load are verbatim `createXml`/`fromXml` with no filtering found, but no test or doc asserts preservation, and `updateLegacyEdit()` rewrites old formats. If properties written onto a `TRACK` or `MIDICLIP` node survive, per-object annotation is cheap; if they do not, everything must live in a separate subtree keyed by `EditItemID`. Test this first — the answer determines the shape of the rest.

3. **Versioning.** Duet's schema will change while Tracktion's own file format also moves under it (the engine has an `updateLegacyEdit()` path of its own). Decide how a Duet project written by an older build opens in a newer one, and what happens when the engine's format changes beneath it. Also decide what "the project" is to the producer: one file, or a folder — the engine supports folder-based projects (`docs/Folder_Based_Projects.md`).

4. **`EditItemID` stability across save/load**, since any Duet-side reference to a track or clip depends on it, and the `applyNewIDsToExternalValueTree` hook exists precisely because IDs can be reassigned. Verify when reassignment happens and what it does to references held outside the engine's tree.

Deliverable: the persistence layering and versioning rules, with the round-trip question answered by experiment rather than inference — ready to become an ADR and to feed the foundation spec (86t5lu).

## Notes

**claude** — 2026-08-09T20:26:13Z

Rider answered at ddp1qt (2026-08-09): a custom property ('duetCustomProp') written on an engine-owned TRACK ValueTree node SURVIVES save/reload — proven through a full te::loadEditFromFile engine reload and through flushState + direct file write. Engine-owned tree nodes can carry Duet's custom properties. Caveat from the same session: the save must use Duet's own path (flushState + direct write), not EditFileOperations::save, which crashes for project-less edits (see ddp1qt closing note, bug 4).

**claude** — 2026-08-09T22:44:43Z

From skb4tp (2026-08-09), directly in this node's domain: Edit::flushState() writes parameter blobs WITH the Edit's UndoManager (AutomatableEditItem::saveChangedParametersToState, tracktion_AutomatableEditItem.cpp:321). Consequence: a save that flushes pollutes the undo history with an unnamed transaction and CLEARS THE REDO STACK if it runs after an undo. Duet's save path (flushState + direct XML write, per ddp1qt) must decide how to neutralize this — save at transaction boundaries, or snapshot without flushing (skb4tp's spike proved the vocabulary's own ops write through immediately, so a digest without flushState was sufficient there). Also: undo/redo can permute ValueTree property order, so any round-trip comparison must canonicalize properties.
