---
id: 1c8sjh
title: 'Persistence core: project folder, DUET tree, snapshot save, in-app New/Open/Save'
state: done
assignee: claude
priority: high
depends_on:
    - quiwf3
parent: b1j3me
created: 2026-08-11T01:50:53Z
updated: 2026-08-18T10:50:45Z
---

## What to build

The persistence facade per spec b1j3me / ADR 0005 and branch prototype/duet-persistence (27/27): a project is a self-contained folder (edit file plus an audio subdirectory, paths project-relative); Duet's own data lives in a DUET child tree in the same file; saving is a snapshot — copy the state, apply the parameter blobs to the copy with no UndoManager, write the copy atomically — never the engine's own flush-and-save path (hazards 3 and 4). EditItemID-derived refs are the durable keys. Wired to minimal app chrome — New/Open/Save and a title-bar dirty marker — so the whole path demos end to end. (Schema versioning and autosave are their own slices.)

## Acceptance criteria

- [ ] Create project → a folder appears containing the edit file and an audio subdirectory; open project → the same state comes back (canonicalized digest of saved state equals digest after reload).
- [ ] Importing an audio file copies it into the project's audio subdirectory and the clip stores a project-relative reference; after moving the whole project folder elsewhere on disk, the project opens and the clip still plays non-silence.
- [ ] Worked example (spec): set a plugin parameter explicitly, let automation diverge it during playback, save, reload → the explicit value is restored exactly; and the in-session redo stack survives the save (redo still works after saving).
- [ ] Duet-side data written into the DUET child tree persists across save/reload; facade refs remain valid for the same items after reload (EditItemID stability).
- [ ] The dirty flag: an Action marks the project dirty (title-bar marker appears); save clears it; undo back to the saved state is not required to clear it.
- [ ] Save is atomic: killing the app mid-save never leaves a corrupt project file (write-then-rename or equivalent, verified by test).
- [ ] In-app demo: New → edit → Save → quit → Open shows the edited project, launched via pw-jack.

## Notes

**claude** — 2026-08-17T04:12:26Z

Clarification (2026-08-17): the New/Open/Save chrome here is deliberately minimal scaffolding to exercise the persistence facade end to end; ce17ym later replaces it with the full Duet-menu lifecycle (Save As, Recent, close prompt, untitled-folder flow). Build it throwaway-thin. The in-app demo criterion is a manual check on the dev machine, recorded as a note — not an automated test.

**claude** — 2026-08-18T10:50:38Z

Done. `duet_persistence` owns the project folder, the snapshot save and the DUET tree; `duet_app` grew the throwaway New/Open/Save chrome that walks it end to end. 26 tests green, format and lint clean.

## What is there

- `Project` — `create`/`open` on a folder, `save()`, `hasUnsavedChanges()`, `importAudioFile()`, `setDuetValue`/`duetValue`. `ProjectLayout` names the three paths: `project.tracktionedit`, `audio/`, and `project.tracktionedit.saving`.
- `tests/ProjectTests.cpp` — 11 cases: one per acceptance criterion, plus the two failure simulations atomicity needs.
- `Main.cpp` — New / Open / Save / Add Track / Play / Stop and the title-bar dirty marker. Deliberately throwaway; ce17ym replaces it.

## Decisions

**The edit file moved into the `Session` constructor.** `Session` used to take a project folder and name the file itself, which made `duet_model` know the layout that `duet_persistence` owns — and persistence depends on model, so the layout could not move where it belongs without a cycle. Now `Session` takes the edit file, and the folder holding it is the project folder that clip source references are stored relative to. `Session::openExisting` is the read path; the plain constructor still creates an empty edit and writes nothing.

**One declared hole in the engine seam: `duet::model_engine_access`.** A save writes the project's whole state, and the engine's state tree *is* that state, so persistence has to reach the `Edit`. It does that through `EngineAccess::editOf` — a header on no public include path, exposed as its own CMake target that only `duet_persistence` links. The facade rule is unchanged everywhere else and still enforced by `duet_tests` linking only the facades. Recorded in `docs/ARCHITECTURE.md`.

**The save is a snapshot, never `Edit::flushState()`** (ADR 0005, hazard 3). `save()` copies `edit.state`, writes each plugin's diverged-parameter blob onto the copy with a null UndoManager, serialises the copy to `project.tracktionedit.saving`, and renames it over the project file. The engine's own flush writes those blobs *through* the UndoManager, which seals a transaction and destroys the redo stack — the worked-example test asserts the redo stack survives a save precisely because of this.

**`stateDigest()` had to learn what the engine changes by itself.** A saved edit and the same edit reloaded are not the same XML: the engine drops `projectID` on read (Duet keeps no ProjectManager for it to mean anything to), moves `SCENES` and `OUTPUTDEVICES` among their siblings, and adds an empty `MODIFIERASSIGNMENTS` to level plugins. The digest now strips `projectID`, drops nodes that carry no property anywhere beneath them, and *stable*-sorts children by type name — stable, so runs of one type keep their order and TRACK order and plugin-chain order still count.

**Two thin fader ops, not 4r7nlj's vocabulary.** The worked example needs an explicit parameter value and automation that diverges from it, so `EditOps` gained `setTrackVolumeDb` and `addVolumeAutomationPoint`, and `Session` gained `trackVolumeDb` (what the producer set) and `liveTrackVolumeDb` (what automation is driving). The full mixer and plugin-parameter vocabulary stays 4r7nlj's.

**Divergence is driven by an offline render, not playback** — hazard 6, the transport dies after the first headless playback. `renderToFile` runs `updateParameterStreams` just the same, so the test diverges the fader deterministically and headlessly.

## Facts a reviewer needs

- **Tracktion drops track names matching "Track \<n\>".** `AudioTrack::resetName()` is called for any such name and `getName()` regenerates it from the track number, so those tracks are stored unnamed on purpose. The app names new tracks exactly that, so a saved file shows unnamed TRACK nodes and still reads back correctly. Names the engine does not generate itself ("Bass", "Keys") are stored verbatim, and the tests assert that.
- **Atomicity needs two complementary simulations**, because one is not enough. A truncated `.saving` file left behind: the project still opens at its last save, and the next save clears the wreckage. A *non-empty* directory at the `.saving` path: the write fails, `save()` returns false, the project file is untouched. An empty directory does not work as a blocker — JUCE's `overwriteTargetFileWithTemporary` rmdirs the target first, which is how the first version of this test passed while proving nothing.
- **The `-Wfloat-equal` suppression in `applyParameterBlobs` is deliberate and local.** Exact inequality is the engine's own test for "automation has moved this parameter off what the producer set"; a tolerance would silently drop a small deliberate change.
- **`juce::String(const char*)` asserts on non-ASCII.** The shell's own text is UTF-8, so `Main.cpp` routes it through a `text()` helper wrapping `juce::CharPointer_UTF8`. Without it the 100 ms status timer fires a JUCE assertion on every tick.

## In-app demo (manual, dev machine, 2026-08-18)

`pw-jack ./build/modules/duet_app/duet_app_artefacts/Debug/Duet` → New in `~/Music/Nocturne` → Add Track ×4 → Save → quit → Open: the edited project came back with its tracks, verified on disk as well as on screen. One bug found and fixed in the process — the dirty marker was a bullet (U+2022) that the window manager's title-bar font cannot draw, so the title shifted by a character with no glyph visible. It is an ASCII `*` now, and the producer confirmed it.

Schema versioning (ne2fhn) and autosave/recovery (3vwusn) are untouched, as intended.
