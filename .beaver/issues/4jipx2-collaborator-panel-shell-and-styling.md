---
id: 4jipx2
title: Collaborator panel shell and styling
state: todo
priority: high
depends_on:
    - nelbwc
parent: 535bbo
created: 2026-08-12T03:51:21Z
updated: 2026-08-12T03:51:21Z
---

## What to build

The right dock, in Graphite: the surface the Collaborator speaks from. A scrolling conversation of producer messages and Collaborator commentary in accent bubbles, a composer at the bottom, a selection context chip on the producer's message showing what was selected when it was sent, quick-prompt chips above the composer that adapt to the current selection, and the Task Run card with its spinner, its "you can keep editing" hint, Cancel, and rotating friendly status phrases. The ✦ badge and the teal accent belong to these surfaces and nowhere else.

This slice owns placement, styling, and the panel's own state — not the mechanics. The Collaborator service, the socket, Task Runs and the Duet Loop are spec js437t's; here the panel is driven by a development-only conversation source that can produce messages, a running Task Run, a cancellation, and a failure, so every state is reachable and reviewable before the service exists.

## Acceptance criteria

- [ ] The panel docks right, toggles with C and from the Duet menu, and its width and visibility persist with the project's view state.
- [ ] Producer messages and Collaborator commentary render distinctly, commentary in accent bubbles, and the conversation scrolls with the newest message in view; a long conversation scrolls without the composer moving.
- [ ] The context chip, worked: with two clips selected, sending a message shows a chip reading two clips on that message; with a track selected it names the track; with nothing selected there is no chip. The chip records the selection at send time and does not change when the selection later changes.
- [ ] Quick-prompt chips change with the selection — a clip selection, a track selection and an empty selection each offer their own set — and clicking one fills the composer without sending.
- [ ] A running Task Run shows the card with a spinner, the keep-editing hint, a Cancel button, and status phrases that rotate; the composer is disabled while it runs and re-enabled when it ends.
- [ ] Cancel ends the run and leaves a "task canceled, nothing changed" line in the conversation.
- [ ] A failed run leaves one plain error line in the conversation, and the rest of the app keeps working — playback, editing and saving are unaffected.
- [ ] Nothing outside this panel and the Suggestion surfaces uses the teal accent token or the ✦ badge.
- [ ] The panel reads and styles only; every state above is reachable from the development-only source with no AI backend, no socket and no network.
- [ ] Typing in the composer suppresses bare-letter shortcuts, and Escape returns focus to the last focused surface without clearing the composer's text.
