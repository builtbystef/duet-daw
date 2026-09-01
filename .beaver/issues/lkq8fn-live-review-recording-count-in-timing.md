---
id: lkq8fn
title: 'Live review: recording count-in timing'
state: todo
priority: low
labels:
    - review
    - roadmap:h0eir5
depends_on:
    - nvdslx
parent: 3xxk8b
created: 2026-09-01T18:40:41Z
updated: 2026-09-01T18:41:15Z
---

## Human/device review

Excluded from the AFK implementation queue.

- [ ] Through `pw-jack`, record one MIDI and one audio take with 1 bar and 2 bars in 4/4, including a take beginning at bar 1.
- [ ] Click/countdown feel aligned; capture begins at the intended boundary; click is absent from take/export.
- [ ] Cancel once from each count-in length and confirm no take/file/Action appears and arm/monitoring remain.
- [ ] Save/reopen preserves choice. Record setup and Target Producer verdict in a note before closure.

On failure, create a bounded `session:task` child under `3xxk8b` with label `roadmap:h0eir5`, add it as this review's dependency, note the failed step, and release the review so the AFK queue can resume.
