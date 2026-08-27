---
id: cwz0of
title: 'The suggest tool: the edit-operation vocabulary and its validation'
state: done
assignee: claude
priority: medium
depends_on:
    - v5yhh1
    - em487d
    - 4r7nlj
    - v6ac5c
parent: js437t
created: 2026-08-12T04:02:52Z
updated: 2026-08-27T07:10:07Z
---

## What to build

The Collaborator's one write-tool, and the closure principle behind it. A call carries a summary and an ordered list of elements; an element is one human-meaningful change carrying one or more edit operations, and it is the cherry-pick unit, so elements must be independently applicable. The operation set mirrors exactly what the Target Producer can do through the milestone-one UI — MIDI notes, clip lifecycle and placement, tracks and routing, mixer values and sends, plugins and their parameters and sidechain source, automation points, tempo and time signature — and nothing in it creates audio content.

Validation happens before anything exists: unknown ids, out-of-range values, and malformed operations come back as an error the model can correct and retry against. A valid call creates a Suggestion through the foundation's mechanism and returns its id, altering nothing in the project. A run may produce commentary, a Suggestion, or both, and at most one new Suggestion.

## Acceptance criteria

- [ ] Worked: a call with one element carrying a mixer operation that sets a track to −3.0 dB returns a suggestion id; the created Suggestion carries that summary, that element description, and that operation; the project digest and the undo stack are unchanged.
- [ ] Every operation domain the spec enumerates round-trips into a Suggestion and reads back identically: MIDI notes, clip lifecycle and placement, track lifecycle and routing, mixer values and sends, plugin add, remove, reorder, parameter and sidechain source, automation points, tempo and time signature.
- [ ] The vocabulary contains no operation that creates audio content: the audio-clip operations that exist are limited to moving, trimming, looping, duplicating, and deleting clips that already exist.
- [ ] Validation, worked: an operation naming a clip id that does not exist is rejected with an error naming the offending id and the operation's position; nothing is created; a corrected retry within the same run succeeds.
- [ ] Validation, worked: a mixer operation with a volume outside the permitted range is rejected the same way, while the same operation at a legal boundary value is accepted.
- [ ] Operations inside one element may target what an earlier operation of that same element creates; an element that depends on another element's creation is rejected, because elements must be independently applicable.
- [ ] A second call within one Task Run is refused with an error and the first Suggestion stands.
- [ ] A run that calls no write-tool produces commentary alone, and a run that produces both commentary and a Suggestion is possible.
- [ ] Built-in plugin parameters are set in real units and external plugin parameters in normalized 0..1, and a value in the wrong domain is rejected rather than silently converted.

## Notes

**claude** — 2026-08-27T07:10:07Z

Built. `suggest` is the Collaborator's one write-tool: it answers over the real
socket against the test-double sidecar, checks every operation against the live
project before anything exists, and makes a Suggestion that alters nothing.

## WHAT LANDED

- `modules/duet_collab/include/duet/collab/SuggestTool.h` + `src/SuggestTool.cpp`,
  in the existing `duet::collab_tools` target — `SuggestTool` and what it makes:
  `collab::Suggestion` (id, summary, elements) over `collab::SuggestionElement`
  (a description, the operations exactly as validation accepted them, and the
  model's applicable form of them). `JsonRpc.h` gains
  `rpcError::suggestionAlreadyMade` (-32003); `toolId` gains `toNote`.
- `duet_model` grew the five things the write side needed and did not have:
  `Session::secondsAtBeats` and `secondsAtBar`, the inverses of the published
  `beatsAtSeconds` and `barAtSeconds`; `Suggestion::moveClip (clip, toTrack,
  start)` and `Suggestion::trimClip (clip, start, length)`, which the spec's
  `clip.move { trackId? }` and `clip.trim { startBar, lengthBars }` need and
  which `EditOps` already had; and `model::faderMinimumDb`/`faderMaximumDb`,
  which `duet::gui::Mixer` now names instead of stating itself.
- `tests/SuggestToolTests.cpp` (10 cases, one per criterion plus one more),
  `tests/ProjectToolsHarness.h` — the Tool Vocabulary harness now registers the
  write tool beside the read ones, collects a run's commentary, and reads a
  Suggestion back — and `tests/sidecar_double`, whose `call-tools` list now takes
  a `{ text }` entry that is commentary rather than a question.
- `docs/ARCHITECTURE.md`, and `docs/GLOSSARY.md` gains **Element** and
  **Edit Vocabulary**.

## THE CONTRACT, AND THE DECISIONS IN IT

A call is `{ summary, elements: [{ description, operations }] }` and answers
`{ suggestionId }`. An operation is an object saying which one it is in `op`,
carrying the fields the spec's table names for it. Twenty-four operations, one
for each of the spec's, and no more.

1. **Placeholders are written `#name`, and they belong to their element.** The
   spec says what each operation takes but not how one names what an earlier one
   makes, and criterion six requires that it can. So an operation that makes
   something carries an optional `ref`, and any id field takes that name in place
   of a project id. A project id never begins with `#` and a placeholder always
   does, so neither end of the seam has to guess which it is looking at. Scoped
   to the element, because an element is the cherry-pick unit: a name that
   another element declared is refused *as that*, and the message says which of
   the two things went wrong rather than reporting a name nothing declared.

2. **Checking and building are one walk.** An operation that cannot be checked
   cannot be built either, so every check happens where its value is wanted and
   the error names the field. The walk runs on the message thread, through the
   same marshal the read tools use. Nothing is kept unless all of it passes: a
   refused call leaves no half-Suggestion behind and does not even take an id,
   so `suggestion-1` is the first one that exists.

3. **What is kept is the model's own form, twice over.** One
   `model::Suggestion` per element, named for the element, and one for the whole,
   named for the summary — so that accepting an element and accepting the whole
   each land as one Action under the right name. The two lists resolve their
   placeholders separately, neither seeing the other's. This is what aw5t9l
   cherry-picks from; the state machine around it is still that issue's.

4. **A value is held to the range the thing it is written to has**, and that
   moved one constant: the fader's travel was `duet::gui::Mixer`'s, which
   `collab_tools` cannot link and should not. It is `duet::model::faderMinimumDb`
   and `faderMaximumDb` now, beside `silentDb`, and the Mixer names it. A
   built-in's parameter is held to its own two ends in the real units v6ac5c
   settled; a scanned plugin's to the 0..1 it speaks itself; so a number written
   in the wrong one of those two domains is refused rather than converted.

5. **Bars and beats convert through the model.** The vocabulary writes
   `startBar`, `lengthBars`, `atBar`, `timeBeats` — what the read tools report —
   and the model edits in seconds, so the two new inverses are where the turn
   happens rather than arithmetic in the tool.

6. **`track.setOutput` takes `track-master` back to no bus at all**, which is the
   inverse of what `list_tracks` does: a track with no destination of its own
   reads as the master, so writing the master has to mean exactly that.

## THE ONE THING THAT CROSSES UNCHECKED

A `plugin.setParam` against a plugin the *same element* is adding. The plugin
does not exist yet, so nothing can be asked what parameters it has or what they
may be. It is stated in `SuggestTool.h` and published as **w1nar1** (bug, low,
under js437t) rather than fixed here: closing it needs the model to state what a
built-in has without an instance of it, which is a decision this issue did not
carry.

## WHAT THE TESTS SAY

All at the seam the spec names, over a real socket and against a real project,
through the same test-double sidecar the read tools use.

- Criterion 1 — a mixer element at −3.0 dB answers with an id; the Suggestion
  carries the summary, the description and the operation; digest, undo names and
  redo names are untouched.
- Criterion 2 — every domain in one call, twenty-four operations over seven
  elements, each reading back byte-identical to what was sent.
- Criterion 3 — five invented audio-creating names are each refused as unknown,
  and the five things that can be done to an audio clip that already exists all
  pass.
- Criterion 4 — `clip-4242` is refused naming the id and
  `elements[0].operations[1]`; the corrected call in the same run is the run's
  only Suggestion; the project never moved.
- Criterion 5 — six dB over the fader's top is refused, the top itself is not.
- Criterion 6 — an element that makes a track, a clip on it and a note in it is
  accepted *and auditions*, which is what independently applicable means; the
  same two operations split across two elements are refused, naming `#pad` and
  saying whose it is.
- Criterion 7 — the second `suggest` of a run comes back
  `suggestionAlreadyMade`, and the first stands unaltered.
- Criterion 8 — a run of commentary alone says its words and makes nothing; a
  run of commentary and a call does both.
- Criterion 9 — the compressor's ratio at 4.0 is accepted and at 0.05, the
  number the engine keeps underneath, is refused; the scanned VST3 fixture's
  first parameter at 0.5 is accepted and at 4.0 is refused.
- And one more, which no criterion asks for but the second operation list would
  otherwise go untested: a two-element Suggestion auditions whole, and its second
  element auditions alone without the first.

## Checks

Format clean. Lint clean. 348 tests pass, with the nine hardware-dependent skips
this machine always has.

## Where TDD was and was not the loop

Criterion one went red first. The rest of the vocabulary did not: twenty-four
operations fully specified by js437t's own table is a mechanical surface, and
writing it in twenty-four red-green slices would have been ceremony over a list
that was already written down. Criterion two's round-trip was written against
the finished vocabulary and passed first time. Every criterion after it drove
real changes.
