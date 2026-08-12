---
id: quiwf3
title: 'Model core: performAction/undo/redo with track and clip ops behind the engine-free facade'
state: todo
priority: high
depends_on:
    - sea14w
parent: b1j3me
created: 2026-08-11T01:50:25Z
updated: 2026-08-11T01:50:25Z
---

## What to build

The Action mechanism and the first two op domains (tracks, clips) of the edit vocabulary, exactly as proven at skb4tp and bound by spec b1j3me / ADR 0004. Every mutation goes through one facade whose public interface exposes zero engine or JUCE types. The prototype's decision, verbatim:

```cpp
using TrackRef = std::uint64_t;   // opaque; wraps EditItemID
using ClipRef  = std::uint64_t;

void performAction(std::string_view name,
                   const std::function<void(EditOps&)>& ops);
bool undo();
bool redo();
```

`performAction` begins a new named transaction, runs the ops synchronously on the message thread, and deliberately does not seal afterwards, so the engine's deferred undo-tracked writes merge into the Action that caused them; the transaction inhibitor is held for long Actions. Raw ops are callable only inside `performAction`. Undo depth is 200, in-memory per session. Every programmatic clip insertion pins its source-file reference deliberately, project-relative — never the engine default. This slice also delivers the two test assets everything downstream leans on: the canonicalized state digest (undo/redo permutes ValueTree property order) and the headless playback workaround (retry play until playing — the transport dies once after first headless playback).

Prior art: branch prototype/undo-vocabulary, 28/28 checks.

## Acceptance criteria

- [ ] Track ops (add, remove, rename, reorder) and clip ops (insert audio clip, move, trim, delete) exist on the facade; the facade's public headers compile with no engine or JUCE include paths.
- [ ] Worked example: `performAction("Add drum loop", ...)` performing five ops (one track add, four clip inserts) produces exactly one new undo step named "Add drum loop"; `undo()` makes the canonicalized digest equal the pre-action digest; `redo()` makes it equal the post-action digest.
- [ ] Undo/redo parity: two consecutive Actions, then undo twice → initial digest; redo twice → final digest.
- [ ] The engine's deferred clip re-sort merges into its causing Action: move a clip so the re-sort fires, undo, and the redo stack still redoes the move (regression against hazard 2).
- [ ] An inserted clip's stored source reference is project-relative inside the project folder — never TEMP-resolved — and the clip renders/plays non-silence (regression against hazard 5).
- [ ] After 201 Actions, the undo stack holds 200: the oldest Action is no longer undoable, the newest 200 are.
- [ ] Raw ops are not callable outside `performAction` — enforced by construction, not convention.
- [ ] `performAction` off the message thread fails loudly (assertion), matching the thread model: the message thread is the sole model writer.
- [ ] The canonicalized digest helper and headless play-retry are available to all future tests, with a test proving digest stability across an undo/redo round-trip.
