---
id: quiwf3
title: 'Model core: performAction/undo/redo with track and clip ops behind the engine-free facade'
state: done
assignee: claude
priority: high
depends_on:
    - sea14w
parent: b1j3me
created: 2026-08-11T01:50:25Z
updated: 2026-08-18T09:00:56Z
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

## Notes

**claude** — 2026-08-18T09:00:56Z

Done. The Action mechanism and the track and clip vocabulary live in duet_model, behind a facade whose public header names no engine or JUCE type; the test target links the facades and the new test-support library only, and its compile command carries no engine include path, so the seam is enforced by the build.

The seam under test is the one the spec names: duet_model's public interface. tests/EditVocabularyTests.cpp holds one test per acceptance criterion (14 tests in the suite; all four checks pass).

What each criterion rests on:
- Vocabulary: EditOps::addTrack/removeTrack/renameTrack/moveTrack and insertAudioClip/moveClip/trimClip/deleteClip. Reads come back through Session::tracks() as TrackInfo/ClipInfo.
- Worked example, parity, depth, re-sort merge, source pinning, thread rule, construction rule, digest stability: one named test each.

Decisions made in the build:
1. Off the message thread, performAction throws std::logic_error rather than firing an assertion. A jassert traps the process in Debug and disappears in Release, so no test could hold the criterion in both configurations; a throw is loud in every build and is what the test asserts.
2. Undo depth. JUCE's UndoManager trims by stored units and never below its minimum transaction count, so it caps nothing on its own: setMaxNumberOfStoredUnits(1, 200) is what turns "at least 200" into "the newest 200". Verified the test discriminates by removing the line and watching it report 201.
3. Hazard 5, precisely. The engine writes a relative source path against the edit FILE and resolves it against the folder that HOLDS the edit file — one level apart, which is why the reference lands on a file that does not exist and the clip plays silence. setToFile(alwaysRelative) reproduces it. Duet therefore writes the reference the engine reads: project-relative for a file inside the project folder, absolute for one outside. Edit::alwaysUseRelativePaths is deliberately not set, since it only makes the engine produce more of the off-by-one paths.
4. The engine creates its SCENES node lazily, when the first track is added, and that node survives the undo that removes the track — which made two states of the same project compare unequal. Session's constructor creates the scene list up front, so the node is in every digest. Preferred over stripping it from the digest, which would have created a blind spot.
5. JUCE_MODAL_LOOPS_PERMITTED=1 joins duet_engine_config (all targets, so the JUCE configuration stays identical everywhere and no ODR violation appears). JUCE gates its only timed message pump behind it, and headless tests need that pump to let the engine's deferred clip re-sort land.
6. Session now takes a project folder and has no default constructor: the model has to know where the project's files are before it can store a reference relative to them. Nothing is written to the folder — project lifecycle stays 1c8sjh's. The app shell points at a scratch folder until then.
7. Session::renderToFile renders offline on the calling thread; the engine's threaded path reports progress through a UIBehaviour a headless test does not have. It is the minimum that proves a pinned clip is not silent; the render harness proper is 6zog6s.
8. tests/support is a small library (duet::test_support) holding the temp project folder, the tone writer, the message pump, the headless play-retry, and file peak measurement. It links the engine PRIVATE exactly as the facades do, so the suites stay engine-free.
9. tests/.clang-tidy switches off two checks for the suites only, in the same spirit as the existing Catch2 exclusions: EnumCastOutOfRange fires inside Catch2's own flag arithmetic, and function-cognitive-complexity measures what TEST_CASE expands into.

For a reviewer: Session::loadDemoContent stays for now — the app has nothing else to play until MIDI ops (4r7nlj) and project open (1c8sjh) land. The headless playback test skips itself when the machine reports no audio device, so it cannot turn into a CI flake before 3u1blw decides what CI's audio looks like.
