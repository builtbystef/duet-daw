---
id: s1jzd4
title: 'Tracks: headers, track lifecycle, and clips on the timeline'
state: todo
priority: high
depends_on:
    - 5he6vd
parent: 535bbo
created: 2026-08-12T03:49:23Z
updated: 2026-08-12T03:49:23Z
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
