---
id: 7tw2tz
title: 'The panel on the real service: conversation, run states, estimate marks, development trace'
state: todo
priority: medium
depends_on:
    - oocnng
    - 2z0y5u
    - 4jipx2
parent: js437t
created: 2026-08-12T04:03:44Z
updated: 2026-08-12T04:03:44Z
---

## What to build

The Collaborator panel stops being driven by its development-only conversation source and starts being driven by the Collaborator service. Sending a message starts one Task Run carrying the opening context — the selection at send time, the playhead position, whether the transport is rolling. Commentary lands in the conversation as it streams. The Task Run card is driven by real events: spinner, the keep-editing hint, rotating friendly status phrases, Cancel.

A canceled run leaves the "task canceled, nothing changed" line; a failed one leaves a single plain error line while the DAW keeps working fully — no offline state, no queue, no retry. Output from a tainted run carries its estimate mark, and opening the mark shows that run's ledger. In development mode only, the raw tool-call trace of a run is visible; the Target Producer sees the friendly phrases instead.

## Acceptance criteria

- [ ] Sending a message starts exactly one Task Run; the composer is disabled while it runs and re-enabled at its terminal event.
- [ ] Commentary appears progressively as it streams, not only when the run finishes.
- [ ] Opening context, worked: with two clips selected and the playhead at bar 9 beat 1 while the transport rolls, the run carries those two clip ids, that position, and playing true; changing the selection after sending does not change what the run carries.
- [ ] Playback, editing, and recording continue unaffected while a run is in flight, asserted with the transport rolling.
- [ ] Cancel ends the run and leaves the "task canceled, nothing changed" line, and no commentary arrives after it.
- [ ] A failed run leaves exactly one plain error line, and saving, playing, and editing all still work afterwards; there is no dedicated offline state, nothing is queued, and nothing retries by itself.
- [ ] Estimate mark, worked: a run that called an estimating tool has its commentary marked, and opening the mark lists the estimated values with their methods and confidences; a run that called none carries no mark.
- [ ] In a development build the raw tool-call trace of a run is inspectable with tool names, arguments, and results; in an ordinary build it is absent and only the status phrases appear.
- [ ] The development-only conversation source is gone from the shipping path.
- [ ] Closure waits for user review.
