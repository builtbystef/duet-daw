---
id: jsfhhg
title: 'Mixer graph controls: groups, post-fader sends, and sidechains'
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

Expose the complete milestone-one mix graph to the Target Producer. Group tracks can be created directly, ordinary tracks can feed them through output routing or post-fader sends, and a sidechain-capable plugin can choose another track as its key input. The Mixer presents these relationships where the producer edits them and the arrangement presents group rows distinctly from clip tracks.

The model and Suggestion vocabulary already carry group creation, sends, and sidechain assignment. This slice closes the producer-parity gap.

## Acceptance criteria

- [ ] Add Track offers Group alongside Audio and MIDI; adding one creates a distinctly styled non-recordable group row and Mixer strip as one Add Group Track Action.
- [ ] Groups accept effect inserts, output routing, automation, mute, and fader/pan edits, but reject clips, instruments, and record arm.
- [ ] Every non-master strip can add and remove a post-fader send to a group and edit its level from silence through +6 dB; each completed gesture is one Action.
- [ ] A send is audibly post-fader: lowering the source fader lowers what reaches the send, while the send level controls its amount independently.
- [ ] The Mixer names each send's destination and value, shows the current routing graph without opening a plugin editor, and keeps controls usable with several groups.
- [ ] A sidechain-capable plugin offers None plus every compatible source track, names the current source, and changes it as one Action; a plugin with no sidechain offers no dead control.
- [ ] Worked graph: create a Reverb group, insert Reverb, send a synth to it, then add a compressor to another track and key it from a kick -> both effects are audible in the expected paths.
- [ ] Routing, sends, and sidechains reject self-reference and every direct or indirect cycle before an Action is emitted.
- [ ] Save/reopen and undo/redo preserve the complete graph exactly, and deleting a source/group/plugin removes or clears relationships that can no longer exist without leaving a broken destination.
- [ ] The same group/send/sidechain operations available to a Suggestion are reachable directly by the producer; a source-level parity test guards the milestone-one Edit Vocabulary against drifting ahead of the interface again.

## Testing seam

Use the Mixer view-model and Action seam for graph edits and cycle rejection. Use feature assertions over offline renders for post-fader send and sidechain audibility. Component tests cover the visible controls and menus, not paint.

## Notes

**agent** — 2026-09-01T18:42:01Z

AFK decomposition: graph policy rog54z -> Group UI 808ncc -> sends oy5ubt and sidechains h9b44n. This issue is a spec container.
