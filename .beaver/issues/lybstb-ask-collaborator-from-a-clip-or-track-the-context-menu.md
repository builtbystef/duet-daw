---
id: lybstb
title: 'Ask Collaborator from a clip or track: the context-menu entry and the implicit context'
state: todo
priority: medium
depends_on:
    - 7tw2tz
    - nelbwc
parent: js437t
created: 2026-08-12T04:04:08Z
updated: 2026-08-12T04:04:08Z
---

## What to build

The one-gesture path to asking about *this thing here*. The ordinary right-click menu of a clip and of a track gains an "Ask Collaborator" entry carrying the ✦ badge. Choosing it makes that item the implicit context of the next message — it becomes what the run's opening context names and what the chip on the producer's message shows — opens the panel if it is closed, and focuses the composer without sending anything.

No floating affordances appear on hover, and clicking a clip still only selects it. Quick-prompt chips follow the implicit context exactly as they follow a selection.

## Acceptance criteria

- [ ] The entry appears in a clip's and a track's ordinary context menu with the ✦ badge, and is absent from context menus of surfaces the Collaborator cannot be asked about.
- [ ] Choosing it opens the panel if closed, focuses the composer, and sends nothing.
- [ ] Worked: right-clicking a clip that is not selected and choosing the entry, then sending "what's wrong here" → the message carries a chip naming that clip and the run's opening context names that clip's id.
- [ ] Right-clicking a track and choosing the entry does the same for that track.
- [ ] Invoking the entry on a clip that is inside the current selection keeps the whole selection as the context; invoking it on a clip outside the selection makes that clip the context.
- [ ] Quick-prompt chips adapt to the implicit context exactly as they adapt to a selection.
- [ ] No hover affordance appears on clips or tracks, and clicking a clip selects it and does nothing else.
- [ ] Dismissing the context menu without choosing the entry changes neither the implicit context nor the focus.
