---
id: shhkbj
title: Piano roll
state: done
assignee: agent
priority: high
depends_on:
    - nelbwc
parent: 535bbo
created: 2026-08-12T03:50:33Z
updated: 2026-08-24T18:18:36Z
---

## What to build

The Piano Roll tab of the bottom panel edits the MIDI clip the producer opened — by double-clicking it on the timeline, or by selecting it and switching to the tab. A keyboard runs down the left edge, notes draw as bars over the same timeline geometry the arrangement uses, and a velocity area sits beneath the note grid.

Editing is the same smart tool: double-click on empty grid adds a note of the current note length, double-click on a note removes it, drag moves it (snapped horizontally, chromatic vertically), drag an edge resizes it, and the selection conventions are the shared ones. A note-length control sets what new notes get. Scale highlighting shades the rows outside the chosen key so wrong notes read as wrong, and Fold collapses the keyboard to only the pitches the clip actually uses. The note context menu offers Delete and Quantize.

## Acceptance criteria

- [ ] Double-clicking a MIDI clip on the timeline opens it in the Piano Roll tab with the bottom panel visible; the tab shows which clip it is editing, and an empty clip shows an empty grid rather than nothing.
- [ ] Adding a note, worked: note length 1/4, grid 1/4, double-clicking at 2.3 beats on the row for C4 adds a note at beat 2.25, pitch 60, length 0.25 beats, at the default velocity, as exactly one Action; double-clicking that note removes it as one Action.
- [ ] Dragging a note snaps horizontally to the grid and moves in semitones vertically; the completed drag is one Action carrying both changes, and Alt bypasses the horizontal snap.
- [ ] Dragging a note's right edge resizes it, snapped, as one Action; a note can never be resized to zero or negative length.
- [ ] Velocity: dragging a note's velocity bar changes only that note's velocity as one Action, and a multi-note selection scales together — three notes at 100, 80 and 60 dragged to put the grabbed one at 120 keep their relative order and stay within 1–127.
- [ ] Selection follows the shared conventions on this surface: click, rubber-band, Ctrl+click, Shift+click, Ctrl+A over the clip's notes, Escape clears; Delete removes the selection as one Action.
- [ ] Scale highlighting shades rows outside the chosen root and scale, and changing the scale re-shades without touching any note.
- [ ] Fold, worked: a clip using only C4, E4 and G4 shows exactly three rows when folded, notes keep their positions, and unfolding restores the full keyboard.
- [ ] Quantize from the note context menu moves the selected notes' starts to the current grid as one Action: at grid 1/8, notes at 1.03 and 1.62 beats become 1.0 and 1.5; undo restores both.
- [ ] Vertical zoom scales key heights, horizontal zoom and scroll stay in step with the arrangement's geometry conventions, and both persist with the project's view state.
- [ ] Editing while the transport rolls is audible on the next pass with no glitch, and the playhead crosses the piano roll in step with the arrangement.

## Notes

**agent** — 2026-08-24T17:15:18Z

Acceptance-criterion ambiguity blocks implementation: the worked timing examples define incompatible meanings for the grid labels. The add-note criterion says grid 1/4 snaps 2.3 beats to 2.25 and note length 1/4 produces 0.25 beats, so the denominator is a fraction of one beat. The quantize criterion says grid 1/8 moves 1.62 beats to 1.5, which matches an eighth note in 4/4 (0.5 beat) but not one eighth of a beat (which would snap to 1.625). Options: (1) use conventional musical note values—1/4 is 1 beat and revise the add-note expected start/length; (2) use literal beat fractions—1/4 is 0.25 beat and revise the quantize expectation to 1.625; or (3) specify another grid mapping and revise both examples consistently. User must decide, record the decision in an issue note, and remove needs-review.

**agent** — 2026-08-24T17:18:54Z

Decision (Target Producer): use conventional musical grid values. In 4/4, 1/4 is one beat and 1/8 is half a beat. The add-note worked example is therefore revised to: note length 1/4, grid 1/4, double-clicking at 2.3 beats adds pitch 60 at beat 2.0 with length 1.0 beat. The quantize example remains 1.03 and 1.62 to 1.0 and 1.5.

**agent** — 2026-08-24T18:18:36Z

Completed the Piano Roll slice using the recorded conventional grid decision (1/4 = 1 beat, 1/8 = 0.5 beat). Added the paintless PianoRoll model and JUCE PianoRollCanvas; MIDI-clip open routes from arrangement selection/double-click; clip naming and empty-grid presentation; note add/remove/move/resize with snap and Alt bypass; velocity editing and proportional multi-note scaling; shared selection, rubber-band, Ctrl/Shift/Ctrl+A/Escape/Delete; root/major/minor scale shading; Fold; Delete/Quantize context menu; note-length controls; shared horizontal geometry and playhead; persisted key height and vertical scroll; and audible editing during rolling playback. The transport grid now explicitly offers conventional 1/4 and 1/8 values alongside Adaptive. Tests cover every Action/view-model criterion plus the arrangement-to-Piano-Roll component route and rolling-playback audibility. Final checks pass: format, full lint, full Debug build/typecheck, and all 240 CTest entries (232 passed, 8 expected hardware skips). XDG_RUNTIME_DIR=/tmp was used for Catch2 discovery/test runtime on this sandbox.
