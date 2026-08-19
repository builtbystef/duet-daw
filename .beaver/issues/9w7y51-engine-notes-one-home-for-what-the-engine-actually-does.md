---
id: 9w7y51
title: 'Engine notes: one home for what the engine actually does'
state: done
assignee: agent
priority: high
labels:
    - maintenance
parent: b1j3me
created: 2026-08-19T10:55:40Z
updated: 2026-08-19T11:08:27Z
---

## Why

Every session that touches `duet_model` spends its opening reading vendored
Tracktion sources to re-learn behaviour the project has already paid for.
Session `nfjr5x` read `WaveInputDevice.cpp`, `TransportControl.cpp`,
`DeviceManager.cpp`, `EditInputDevices.cpp`, `SourceFileReference.cpp`,
`EngineBehaviour.h` and `ClipOwner.cpp` before writing a test; `di0frj` opened
by reading `DeviceManager.cpp` again, for the same fact.

The facts are not unrecorded. They are recorded in three places with three
different loading rules, and no session loads all three:

- **Spec `b1j3me`, "Further Notes"** — hazards 1 to 8, the canonical list. Read
  only when the issue being built has `b1j3me` as its parent.
- **`docs/ARCHITECTURE.md`** — prose, read every session per `/implement` step
  2, but it is a map of Duet's seams and carries engine facts only where a seam
  exists because of one.
- **The closing notes of eight done issues** — `skb4tp`, `rquzdc`, `quiwf3`,
  `1c8sjh`, `sohgf4`, `vhl9d0`, `nfjr5x`, `di0frj`. Never read. This is where
  the precise characterisations live: `di0frj`'s note names the rebuild as
  `DeviceManager::applyNewMidiDeviceList` on a four-second timer set by
  `initialise`, and measures the second rebuild five milliseconds behind the
  first. Nothing outside that note knows it.

The canonical list is also stale. Hazard 6 still prescribes "headless tests
retry `play()` until `isPlaying`", which `sohgf4` replaced with the model's own
keeper and `di0frj` replaced again with the take's pre-roll. An implementer who
reads it today gets a remedy two generations out of date.

## What to build

`docs/ENGINE_NOTES.md`: the engine's behaviour, one fact per entry, as the
single home for all of it. Format rules at the top of the file, as
`docs/GLOSSARY.md` does. Each entry says what the engine does, where in the
engine it does it, how the fact was proved, and what Duet does about it.

The eight hazards **move** here rather than being copied — `b1j3me`'s Further
Notes section is replaced with a pointer, so there is one list and not two.
Hazard numbering is kept, because fifteen code comments cite it by number
(`Recording.cpp:271`, `SessionImpl.h:347`, `EditOps.cpp:374`, and the rest).

The wiring is a line in `AGENTS.md` under "Project docs & tracker", the same
shape as the glossary and coding-standards entries. It cannot be an edit to
`.agents/skills/implement/SKILL.md`: that tree is a hash-locked package synced
from `builtbystef/skills` (`skills-lock.json`), so a local edit is overwritten
on the next sync. `AGENTS.md` is loaded every session through `CLAUDE.md`.

The seed inventory — facts the project has already paid for, with their sources:

1. Hazards 1 to 8, from `b1j3me`, with 6 rewritten to its current remedy.
2. The device rebuild, measured: `applyNewMidiDeviceList`, four-second timer
   from `DeviceManager::initialise`, clears and reloads every playback
   context's devices; `checkDefaultDevicesAreValid` schedules a second rebuild
   about five milliseconds behind the first; the only sign the engine offers
   that the build has happened is a non-empty MIDI input list (`di0frj`).
3. `Edit::undo()` stops a running recording before it reverts anything, which
   is why Duet drives the project's `UndoManager` directly (`nfjr5x` note 1,
   already in `ARCHITECTURE.md`).
4. `EngineBehaviour::getFileForNewAudioRecording` is the hook for take paths
   (`nfjr5x`).
5. `HostedAudioDeviceInterface` is the device seam; it is what
   `playWithoutAudioDevice` switches the device manager to, and what the
   engine's own `tracktion_EnginePlayer.h` uses (`vhl9d0`).
6. The engine builds two different graphs.
   `createNodeForEdit(EditPlaybackContext&, ...)` is playback and wraps a track
   with no output and no destination in a `SinkNode` that blocks its audio;
   `createNodeForEdit(Edit&, ...)` is the offline render and sums the same
   track into the master (`vhl9d0`, ADR 0006 amendment).
7. `insertWaveClip` writes a source with `PathStyle::chooseBest` —
   `getRelativePathFrom(editFile)` — and the read is
   `getEditFileFromProjectManager(edit).getChildFile(source)`, one level apart
   (`quiwf3` note 3, `nfjr5x` note 3). This is hazard 5, stated precisely.
8. `Edit::flushState()` flushes every plugin unconditionally
   (`tracktion_Edit.cpp:1176`), and the blob write goes through the
   UndoManager exactly when a parameter's `currentValue != currentExplicitValue`
   — so it is undo-neutral until automation has driven the parameter
   (`rquzdc`). This is hazard 3, stated precisely.
9. Every clip the engine writes as a recording lands goes through the Edit's
   own `UndoManager` — `ClipOwner`'s `addChild (clipState, -1,
   &edit.getUndoManager())` — which is what lets one open transaction collect a
   whole take (`nfjr5x` note 2).
10. `Destination::recordEnabled` refers to its property with a null
    UndoManager, so an undo can never disarm a track mid-take (`nfjr5x` note 5).
11. The engine hands out input instances only through an allocated playback
    context, so a setter must allocate one; reads come straight out of the
    Edit's `INPUTDEVICES` state instead, so that asking a question about a
    track does not open the machine's audio hardware (`nfjr5x` note 6).
12. `EditFileOperations::save` segfaults for project-less edits
    (`EditSnapshot::refresh` null-deref, upstream). This is hazard 4.

## Acceptance criteria

- [ ] `docs/ENGINE_NOTES.md` exists, states its format rules at the top, and
      carries the twelve seed entries above, each with what the engine does,
      where, how it was proved, and what Duet does about it.
- [ ] The hazard numbering is preserved, and every code comment that cites a
      hazard by number still names a hazard that exists under that number.
- [ ] `b1j3me`'s "Further Notes" hazard list is replaced with a pointer to the
      doc, so the list exists in exactly one place.
- [ ] Hazard 6's entry describes the remedy the code actually uses today: the
      playback keeper (`sohgf4`) and the take's pre-roll (`di0frj`).
- [ ] `AGENTS.md` names the file under "Project docs & tracker", alongside the
      glossary and the coding standards. No file under `.agents/skills/` is
      edited.
- [ ] A reader of the doc alone can answer why `Session::startRecording` waits,
      why undo does not go through `Edit::undo()`, and why a group bus with no
      output is silent on playback but audible in a render — without opening an
      engine source or a closed issue.

## Notes

**agent** — 2026-08-19T11:08:26Z

Done. The engine's behaviour has one home.

**What is there.** `docs/ENGINE_NOTES.md` — format rules at the top, hazards 1–8 (numbers unchanged), then the further facts the seed inventory paid for that are not already a numbered hazard. `AGENTS.md` names the file under Project docs & tracker. Spec `b1j3me`'s Further Notes list is now a pointer.

**Decisions.** Seed items 2, 7, 8 and 12 are the precise characterisations of hazards 6, 5, 3 and 4 — they live in those hazard entries, not as a second list. The remaining unique facts (Edit::undo() stopping a take, the take-path hook, HostedAudioDeviceInterface, the two graphs, ClipOwner's undo-tracked add, Destination::recordEnabled, input instances vs INPUTDEVICES reads) are named headings under Further facts, so they cannot be mistaken for new hazard numbers. ARCHITECTURE.md was left alone: it is a map of Duet's seams, and this issue asked for one new home, not two.

**Hazard 6** describes the remedies the code uses today: the playback keeper (`sohgf4`) and the take's pre-roll (`di0frj`). The stale "headless tests retry play() until isPlaying" line is gone.

**Criteria, against the diff.**
1. The doc exists, has format rules, and carries all twelve seed entries (8 hazards + the 7 further facts; 2/7/8/12 folded into 6/5/3/4).
2. Hazards 1–8 still exist under those numbers. Every code comment that cites a hazard cites 3, 5 or 6.
3. `b1j3me` Further Notes is a pointer.
4. Hazard 6 names the keeper and the pre-roll.
5. `AGENTS.md` has the entry. No file under `.agents/skills/` was edited.
6. From the doc alone: `startRecording` waits because `TransportControl::record` cannot be asked twice (hazard 6); undo skips `Edit::undo()` because that stops a running take first; a group bus with no output is silent on playback (`SinkNode`) and audible in a render (the other `createNodeForEdit`).

No code, no tests: this is a documentation move. Format dry-run clean; lint and the suite do not apply.

To confirm: open `docs/ENGINE_NOTES.md` and answer the three questions in criterion 6 without opening an engine source or a closed issue.
