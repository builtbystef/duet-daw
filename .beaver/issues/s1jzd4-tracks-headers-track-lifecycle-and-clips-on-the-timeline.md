---
id: s1jzd4
title: 'Tracks: headers, track lifecycle, and clips on the timeline'
state: done
assignee: agent
priority: high
depends_on:
    - 5he6vd
    - nfjr5x
    - wdt64u
parent: 535bbo
created: 2026-08-12T03:49:23Z
updated: 2026-08-24T03:43:45Z
---

## What to build

The arrangement grows tracks. Each track has a header carrying its name, mute, solo, record-arm, colour, and a drag-to-resize height; the lanes beside the headers draw that track's clips at their positions and lengths, in the track's colour, MIDI clips showing their note content and audio clips their waveform. A "+ Add Track" row is pinned under the last header and offers Audio or MIDI.

Track lifecycle is producer-driven from the header: drag a header to reorder, double-click the name to rename, and a header context menu offers Rename, Duplicate, Delete, and the colour picker. Every one of these gestures ends as exactly one named Action in the model — nothing here reaches around the vocabulary layer — and every context-menu entry mirrors an Action that also has a direct gesture. Track heights persist per project in the VIEW state.

## Acceptance criteria

- [ ] Tracks render as headers plus lanes; a clip's drawn left edge and width match the geometry view-model's mapping of its start and length, and its fill is the track's assigned colour.
- [ ] "+ Add Track" adds an audio or MIDI track below the last one as one Action named for the addition; undo removes it and redo restores it.
- [ ] Reordering, worked: with tracks A, B, C, dragging C's header above B produces the order A, C, B as exactly one Action; undo restores A, B, C.
- [ ] Double-clicking a header name edits it in place; committing emits one rename Action, Escape during the edit cancels with no Action, and bare-letter shortcuts do not fire while the name field has focus.
- [ ] The header context menu offers Rename, Duplicate, Delete, and colour; Duplicate copies the track with its clips and plugins as one Action, and Delete removes it as one Action, both digest-exact on undo.
- [ ] Colour: assigning one of the eight track colours updates the header and every clip on the track, is one Action, and survives save and reopen.
- [ ] Mute, solo, and record-arm toggle on the header and are reflected in the model; muting a track silences it during playback.
- [ ] Height, worked: dragging a header's bottom edge from 80 px to 120 px changes only that track's height; save and reopen returns 120 px, and the resize is not on the undo stack and does not dirty the document.
- [ ] A track added while a project is open gets a view entry with the default height, and a deleted track's view entry does not outlive it.
- [ ] Vertical zoom (Ctrl+Shift+scroll) scales track heights together and persists with the rest of the view state.

## Notes

**claude** — 2026-08-17T04:09:59Z

Scope note (2026-08-17): the header's record-arm toggle drives the arm op from foundation slice nfjr5x (now a dependency); this slice does not implement arming itself.

**claude** — 2026-08-20T10:54:53Z

Waiting for this slice, left by 5he6vd (the arrangement's geometry and canvas):

- `ArrangementCanvas` paints the ruler and the grid and nothing down the canvas yet. Tracks are laid out under the ruler, in `ViewState`'s per-track rows.
- A vertical zoom (Ctrl+Shift+scroll) is `ViewState::scaleTrackHeights`, which grows every row the view holds at once. A view with no rows in it has nothing to grow, so the gesture is real but invisible until tracks put rows there.
- The vertical scroll (`ViewState::vScrollPx`) is held at zero from below and nothing from above: the upper bound is the content height, which is the sum of the track rows this slice lays out. `ArrangementView::scroll` is where that clamp goes.
- `ArrangementView::setWidthPx` takes the canvas width alone for the same reason — the height matters to nothing until there are tracks down it.

**agent** — 2026-08-24T03:19:00Z

Implementation reached the final checks: track/header/lane drawing, MIDI-note and audio-waveform clip content, add/reorder/rename/duplicate/delete/colour/mute/solo/arm gestures, per-track VIEW rows/heights, vertical zoom/scroll clamping, model colour persistence and exact track duplication, app/session wiring, and seam tests are present in the working tree. Format and the full lint sweep pass; 167/168 Debug tests pass (plus expected skips). The sole failure is the pre-existing headless MIDI recording failure captured as wdt64u, reproduced after stashing this entire change and rebuilding untouched HEAD. That required check blocks completion, so wdt64u must be fixed first; then rerun the full checks and finish this issue.

**agent** — 2026-08-24T03:43:45Z

Completed the arrangement track slice. Headers and lanes now present track state and clip geometry, including MIDI notes and audio thumbnails; the add row, drag reorder/resize, in-place rename, context actions, colour, mute/solo/arm controls all route through the model/view seams. Added track colour persistence and exact track duplication to the vocabulary, synchronized per-track VIEW rows, clamped vertical scrolling, and wired the open Session through the app shell. Tests cover the Action and paintless view-model seams, exact undo/redo, persistence, playback mute, heights, and vertical zoom. No new design decisions were needed. Checks: clang-format dry-run passed; full lint passed; full Debug build passed; ctest passed 168/168 with 8 expected hardware skips. The build's Catch2 discovery needed TMPDIR and XDG_RUNTIME_DIR pointed at build/tmp because this harness could not open its configured /run/user/1000 temporary output; the resulting executable and full suite passed.
