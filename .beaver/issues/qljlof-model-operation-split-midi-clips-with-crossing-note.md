---
id: qljlof
title: 'Model operation: split MIDI clips with crossing-note fragments'
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

Extend the split operation to MIDI clips with a fully specified boundary policy.

## Settled note policy

- The original ref remains left; returned ref is right. Split must be strictly inside the clip.
- Notes ending at/before the boundary stay left. Notes starting at/after it move right with starts relative to the right clip.
- A note crossing the boundary becomes two fragments: left ends exactly at the boundary; right starts at beat zero with the remaining length, same pitch and velocity. The right fragment retriggers—Duet has no tie primitive—but no musical duration is discarded.
- Clip name/colour copy; looped MIDI preserves the content phase by materializing the note content heard on each side rather than resetting the right half to loop beat zero.
- Zero-length fragments are never created.

## Acceptance and tests

- [ ] Literal before/at/after/crossing/overlapping note sets produce exact refs, starts, lengths, pitches, velocities, and selection-independent order.
- [ ] Looped MIDI at a non-loop-aligned split preserves the sequence heard on both sides for at least two repeats.
- [ ] Invalid boundaries write nothing; one enclosing Action undoes/redoes digest-exactly.

Use public model seams and independent literal expected values in `ClipOpsTests.cpp`/`MidiOpsTests.cpp`. Run all AGENTS.md checks before closing.
