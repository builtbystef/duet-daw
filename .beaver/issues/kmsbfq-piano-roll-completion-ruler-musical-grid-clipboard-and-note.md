---
id: kmsbfq
title: 'Piano Roll completion: ruler, musical grid, clipboard, and note audition'
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

Complete the Piano Roll as a focused MIDI editor. It gains a musical-time ruler and visible vertical grid, note clipboard commands that follow focus, and audition feedback from both the keyboard and note-edit gestures.

## Acceptance criteria

- [ ] The Piano Roll draws bar numbers plus bar/beat/fine vertical lines from the same `TimelineGeometry` and grid choice as the arrangement; scrolling and zooming keep ruler, notes, playhead, and velocity bars aligned.
- [ ] The ruler is pinned above the note grid and scrubs the project playhead without creating an Action.
- [ ] Ctrl+X/C/V/D act on selected notes when the Piano Roll has focus and on arrangement clips when the arrangement has focus; no command is routed to the hidden surface.
- [ ] Copy/paste preserves each note's relative start, pitch, length, and velocity. Pasting at the playhead is one Action and selects the new notes; one undo removes all of them.
- [ ] Duplicate places the copied phrase one selection-length later on the current grid as one Action; an invalid or empty selection does nothing.
- [ ] Clicking or dragging a piano key auditions that pitch through the track's current instrument and stops it on release, with no project change or undo entry.
- [ ] Adding or moving a note auditions its pending pitch at a bounded level and does not leave a stuck note after release, Escape, focus loss, project replacement, or transport stop.
- [ ] Audition coexists with playback and hardware MIDI input without stealing recorded notes or entering a take.
- [ ] Quantize is available as a visible control as well as the context menu, clearly states the active grid, and remains one Action over the selected notes.
- [ ] Fold, scale highlighting, vertical zoom, selection, velocity editing, and the existing drag preview still work with the ruler/grid and clipboard additions.
- [ ] The focused surface and audition controls are keyboard reachable and expose accessible names.

## Testing seam

Grid/ruler geometry and clipboard behavior are paintless. A small note-audition seam accepts note-on/note-off so its lifetime can be asserted without synthesising UI events; audio feature assertions prove the current instrument sounds. Component tests cover focus-sensitive dispatch and stuck-note cleanup.

## Notes

**agent** — 2026-09-01T18:42:01Z

AFK decomposition: ruler/grid jg62kc, clipboard/focus/quantize jk80m7, and note audition ehdor9 are independent executable leaves. This issue is a spec container.
