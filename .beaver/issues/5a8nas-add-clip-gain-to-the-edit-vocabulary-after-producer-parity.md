---
id: 5a8nas
title: Add clip gain to the Edit Vocabulary after producer parity
state: todo
priority: low
labels:
    - session:task
    - roadmap:h0eir5
depends_on:
    - wi3f34
parent: stoai7
created: 2026-09-01T18:40:08Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Add clip gain to Suggestions only after the direct badge/command exists.

## Acceptance and tests

- [ ] Schema value is dB in the same -60..+24 producer range and targets a durable audio clip ref.
- [ ] Invalid/MIDI/deleted targets stale the Element; values clamp exactly as direct edits.
- [ ] Audition previews gain without dirty/undo, accept is one Action, and reject/revert is digest-exact.
- [ ] Parser/serializer, stale detection, Suggestion rendering summary, and `docs/PRODUCER_OPERATIONS.md` update together.
- [ ] Existing Suggestion seams prove one-Action acceptance and exact audition/revert.

Run all AGENTS.md checks before closing.
