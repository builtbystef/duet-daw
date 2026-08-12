---
id: lxt41c
title: 'Research: the Basic Pitch model weights'' licence'
state: todo
priority: high
labels:
    - research
parent: js437t
created: 2026-08-12T04:01:02Z
updated: 2026-08-12T04:01:02Z
---

## What to build

The analysis layer's one ML dependency ships model weights, and a repository's stated licence does not automatically cover the weights it distributes. Establish from primary sources what licence the weights carry, whether redistributing them inside the AGPLv3 application is permitted, whether a future closed commercial edition could ship them, and what attribution or notice obligations come with them.

If the answer forecloses either path, name at least one alternative — a differently licensed model, or shipping without polyphonic transcription — with its own licence checked. The conclusion is recorded here and must be enough to unblock or cancel the transcription slice without re-reading the sources.

## Acceptance criteria

- [ ] The weights' licence is established from primary sources — the model repository's own licence files, model card, and any accompanying terms — with each claim cited to its source.
- [ ] The finding states plainly whether redistribution inside an AGPLv3 application is permitted.
- [ ] The finding states plainly whether a future closed commercial edition could ship the same weights, per the project's licensing posture.
- [ ] Attribution or notice obligations, if any, are listed concretely enough to implement.
- [ ] If either path is foreclosed or unclear, at least one alternative is named with its licence checked to the same standard.
- [ ] The conclusion is recorded on this issue, and the transcription slice can be unblocked or cancelled from it alone.
