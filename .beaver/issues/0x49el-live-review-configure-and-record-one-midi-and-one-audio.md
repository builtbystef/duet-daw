---
id: 0x49el
title: 'Live review: configure and record one MIDI and one audio track'
state: todo
priority: high
labels:
    - review
    - roadmap:yfpnps
depends_on:
    - 7sd7k2
    - hs7owx
parent: uxkosp
created: 2026-09-01T18:33:36Z
updated: 2026-09-01T18:41:15Z
---

## Human/device review

This issue is deliberately excluded from the AFK implementation queue.

- [ ] Through `pw-jack`, add fresh MIDI and audio tracks and configure each entirely from the arrangement, then repeat from the Mixer.
- [ ] Record a hardware MIDI take and a non-default hardware audio input with While Armed monitoring; each lands once on the intended track and monitoring is audible only under the selected mode.
- [ ] Unplug/disable the chosen device: the track names it as unavailable, disarms, and never substitutes another device. Restore it and confirm the choice returns.
- [ ] Verify arrangement and Mixer remain mirrored throughout.
- [ ] Record hardware, device names, steps, and verdict in a note. Closure requires Target Producer approval.

On failure, create a bounded `session:task` child under `uxkosp` with label `roadmap:yfpnps`, add it as this review's dependency, note the failed step, and release the review so the AFK queue can resume.
