---
id: uxkosp
title: 'Track I/O: input, monitoring, routing, arm, and record state'
state: todo
priority: high
labels:
    - spec
    - roadmap:yfpnps
parent: yfpnps
created: 2026-09-01T18:07:43Z
updated: 2026-09-01T18:42:01Z
---

## What to build

Complete the per-track recording and routing surface. An audio or MIDI track shows what feeds it, how that input is monitored, where its output goes, whether it is armed, and whether a take is currently rolling. The detailed controls are available from the track's visible chrome and stay in step wherever the same fact appears in the arrangement and Mixer.

This is the interface over the recording vocabulary built at nfjr5x and the output routing already present in the Mixer. It does not redesign the engine path.

## Acceptance criteria

- [ ] Every audio and MIDI track visibly names its selected input and output; a group visibly reads as a summed bus and cannot be armed.
- [ ] The input picker offers only compatible inputs: audio channels for audio tracks and enabled MIDI inputs for MIDI tracks, plus an explicit None choice.
- [ ] The monitoring picker exposes Off, While Armed, and On and always reflects the mode currently held by the selected input.
- [ ] Arm is available in the arrangement and Mixer and both surfaces update immediately when either one changes it; no input is a visible, actionable state rather than a silent record failure.
- [ ] Output routing offers Main Output and every cycle-safe group destination, identifies the current destination, and selecting it again emits no Action.
- [ ] Input, monitoring, and arm changes do not enter producer undo. An output change is one named Action and one undo restores it.
- [ ] A track added after launch can be fully configured and recorded without reopening the project or visiting a machine-wide settings page.
- [ ] Worked audio path: add an audio track, choose a non-default channel, choose While Armed, arm, record, and stop -> one take lands on that track and monitoring is audible while it rolls.
- [ ] Worked MIDI path: add a MIDI track, choose an enabled hardware input, arm, record notes, and stop -> one MIDI take lands on that track and plays its instrument.
- [ ] An unavailable input after a device change is shown as unavailable and disarms or refuses recording plainly; it is never silently replaced by a different input.
- [ ] The controls follow Graphite in both themes, are keyboard reachable, and carry accessible names and concise tooltips.

## Testing seam

Drive compatibility, state, no-undo configuration, output Actions, and device-loss behavior through a paintless Track I/O view-model. Component coverage asserts composition, focus, and the mirrored arrangement/Mixer controls. The two worked recording paths use the existing recording harness plus one live-device review.

## Notes

**agent** — 2026-09-01T18:42:01Z

AFK decomposition: oscfrz (shared model/view-model) -> 7sd7k2 (arrangement) and hs7owx (Mixer); physical-device approval is review 0x49el. This issue is now a spec container, not claimable work.
