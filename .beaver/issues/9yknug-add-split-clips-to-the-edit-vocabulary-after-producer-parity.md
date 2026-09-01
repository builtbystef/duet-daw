---
id: 9yknug
title: Add split clips to the Edit Vocabulary after producer parity
state: todo
priority: low
labels:
    - session:task
    - roadmap:h0eir5
depends_on:
    - oq1pt4
parent: 7zuqxx
created: 2026-09-01T18:39:07Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Only after the direct route exists, add split to Suggestions and the producer-operation parity inventory.

## Acceptance and tests

- [ ] A split operation names a durable clip target and an absolute musical beat strictly inside it.
- [ ] Audition/accept resolve against current state; a moved/deleted clip or no-longer-interior boundary makes the Element stale rather than splitting a different location.
- [ ] One accepted Element containing several splits lands as one named Action and one undo; rejection remains trace-free.
- [ ] Tool schema, parser/serializer, apply/revert, stale detection, tests, and `docs/PRODUCER_OPERATIONS.md` update together.
- [ ] Existing Suggestion seams prove digest-exact audition/revert and stale behavior.

Run all AGENTS.md checks before closing.
