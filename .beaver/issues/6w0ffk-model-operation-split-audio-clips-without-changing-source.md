---
id: 6w0ffk
title: 'Model operation: split audio clips without changing source phase'
state: todo
priority: low
labels:
    - session:task
    - roadmap:h0eir5
depends_on:
    - jpv27l
parent: 7zuqxx
created: 2026-09-01T18:39:07Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Add `EditOps::splitClip` behavior for audio clips only. The original ref remains the left clip; the returned new ref is the right clip.

## Settled policy

- Split time is absolute project seconds and must be strictly inside the clip. Invalid/non-audio calls return `noClip` and write nothing.
- Left ends at the split. Right starts there, ends at the original end, and advances content offset/loop phase so playback on either side uses exactly the samples the unsplit clip would have used.
- Name, source reference, colour, loop state/length, clip gain, and applicable edge fades copy. The left keeps its fade-in and loses fade-out at the cut; the right loses fade-in and keeps fade-out. Explicit crossfade links are not copied.
- Source file is neither copied nor modified.

## Acceptance and tests

- [ ] Plain, left-trimmed, looped, gained, and faded fixtures produce exact timeline/content properties.
- [ ] An offline render before/after split differs by at most floating render tolerance at the cut and over one loop cycle.
- [ ] One enclosing `Split Clip` Action undoes/redoes digest-exactly with stable left/right refs.

Start in `Session.h`, `EditOps.cpp`, and `ClipOpsTests.cpp`; use ADR 0006 features/render comparison only within the same process. Run all AGENTS.md checks before closing.
