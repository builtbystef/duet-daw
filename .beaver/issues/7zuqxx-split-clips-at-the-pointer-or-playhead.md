---
id: 7zuqxx
title: Split clips at the pointer or playhead
state: todo
priority: low
labels:
    - spec
    - roadmap:h0eir5
depends_on:
    - jpv27l
parent: h0eir5
created: 2026-09-01T18:09:16Z
updated: 2026-09-01T18:42:01Z
---

## What to build

Split selected audio or MIDI clips non-destructively at a snapped pointer position or at the playhead. Both resulting clips refer to the original content with offsets and lengths that make playback continuous; no source file is rewritten.

## Acceptance criteria

- [ ] A visible Split route and its shortcut act on the selected clips at the playhead; a context-menu route splits the clip under the pointer at that pointer's snapped time, with Alt bypass.
- [ ] Splitting one audio clip creates two clips whose combined timeline coverage and heard content equal the original, including a trimmed or looped source.
- [ ] Splitting one MIDI clip distributes notes without losing sustained notes at the boundary; the documented boundary policy preserves what was heard before the split.
- [ ] A multi-selection splits every clip crossed by the chosen time as one Split Clips Action; clips not crossed are unchanged.
- [ ] The split is one Action, one undo restores the original clip digest-exactly, and redo restores the same two identities/content relationships expected by the model contract.
- [ ] A split at or outside a clip edge does nothing, and splitting never modifies or duplicates the source audio file.
- [ ] Suggestions gain the split operation only after the producer route and model operation exist, preserving Edit Vocabulary parity.

## Notes

**agent** — 2026-09-01T18:42:01Z

AFK decomposition: audio model 6w0ffk and MIDI model qljlof -> arrangement routes oq1pt4 -> Suggestion parity 9yknug. Crossing MIDI notes split into two same-pitch/velocity fragments; the right retriggers because Duet has no tie primitive.
