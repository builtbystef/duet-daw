---
id: rquzdc
title: Where does Duet's own project data live relative to Tracktion's Edit file, and how does it version?
state: done
assignee: claude
priority: medium
labels:
    - roadmap:d9gioe
    - session:prototype
depends_on:
    - lf8tnt
    - ddp1qt
parent: d9gioe
created: 2026-08-08T01:29:32Z
updated: 2026-08-09T23:29:05Z
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

**claude** — 2026-08-09T23:29:05Z

RESOLVED by prototype (branch prototype/duet-persistence, 27/27 checks pass). Verdicts are the user's, given 2026-08-09.

THE FOUR DECISIONS:
1. LAYERING — Duet's data lives in a DUET child tree under edit.state (one file, atomic save, persists automatically). Each write chooses undo visibility: nullptr UM for Duet facts invisible to producer undo, the Edit's UM for facts that should revert with it. Coupling to the engine file is mitigated by the schema stamp (below).
2. SAVE PATH — snapshot save, never flushState: copy edit.state (createCopy), apply the IDs::parameters blobs to the COPY with nullptr UM (same format as AutomatableEditItem::saveChangedParametersToState, tracktion_AutomatableEditItem.cpp:307 — writeString(paramID)+writeFloat(explicitValue) for each param whose current != explicit, node located in the copy by itemID), write the copy. Proven: redo survives, explicit values round-trip exactly.
3. VERSIONING — DUET tree carries duetSchemaVersion (int). On load: migrate oldest-first, one step per version, before anything reads the tree (v1→v2 demonstrated). A file NEWER than the app: refuse to open, naming the needed version. Engine format drift is the engine's own problem (its appVersion stamp + updateLegacyEdit path); Duet must set the application version in Engine's PropertyStorage — the spike saved appVersion="Unknown".
4. PROJECT SHAPE — a folder: the edit file plus an audio/ subdir for recordings and imports. (The spike pinned paths absolute per ddp1qt; production uses project-relative paths within the folder.)

EMPIRICAL FACTS the decisions rest on:
- EditItemID is a safe durable key: every track/clip ID survived duetSave + full engine reload, identical across repeated loads. Reassignment happens ONLY on actual ID collision in a file, and the engine keeps the ORIGINAL's ID and reassigns the duplicate (forced-duplicate experiment). Duet-side references keyed by itemID (annotations, sidecar-style maps) are stable.
- The skb4tp flushState hazard is now precisely characterized: Edit::flushState() flushes EVERY plugin unconditionally (tracktion_Edit.cpp:1176), and the blob write (WITH the UM → clears redo, adds unnamed undo step) fires exactly when a parameter's currentValue != currentExplicitValue — i.e. when automation has driven it, e.g. right after playback through an automated section. Without divergence a flush is undo-neutral. Tracktion's own save (EditFileOperations) therefore has this flaw; Duet's snapshot save dodges it entirely.
- A no-flush save LOSES nothing for plain property writes (setVolumeDb wrote through immediately) — the blob only matters for the automation-diverged explicit values.
- ValueTree preserves unknown properties and children verbatim through save/load, so forward-compat "preserve what you don't understand" is free; refusal is still chosen for too-new files because an older build's edits can invalidate future-schema facts it cannot see.

Prototype: prototype-persistence/ on branch prototype/duet-persistence — experiments E1 (ID stability + forced duplicate), E2 (DUET tree + undo interplay + sidecar comparison), E3 (hazard repro + boundary flush + snapshot save), E4 (engine stamps + migration + too-new detection).

Feeds the foundation spec (86t5lu). Ready to become an ADR alongside skb4tp's.
