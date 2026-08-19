---
id: 6629zo
title: 'Voices sound that nobody asked for: a tone at the loop seam, and one stuck after fast undos'
state: todo
priority: high
labels:
    - bug
depends_on:
    - vhl9d0
parent: b1j3me
created: 2026-08-19T05:04:16Z
updated: 2026-08-19T05:04:16Z
---

## What is wrong

Two artifacts reported while listening to 4r7nlj's demo, both tonal, both surviving every fix that issue made:

1. **A faint tone at the start of every loop pass**, "separate from the arpeggiator". It appears after the first or two undos and not on the way forward.
2. **A constant tone at a different pitch**, introduced by clicking Undo several times quickly while the transport rolls, and then sounding indefinitely.

The second is almost certainly a MIDI voice whose note-off never arrived — the shape of a hanging note. The first may be the same fault heard quietly: an undo rebuilds the playback graph, and a voice sounding across the rebuild has nothing left to tell it to stop.

## What was already ruled out

Three hypotheses were tested against the first artifact during 4r7nlj and all three were wrong. They are recorded so the time is not spent again:

- **The demo's copy sitting on the loop seam.** Step 2 duplicated the phrase to bar 5, which is exactly where the transport wraps. Moving it to bar 9 did not change what the producer heard. (The copy stays at bar 9 anyway — a clip on the seam is fragile whether or not it was audible.)
- **The transport loop frozen in seconds while a tempo change moved the music.** The loop follows the tempo forwards; the bug was that it did not come back on the undo, leaving the transport wrapping a beat and a half early and cutting a note every pass. That was real and is fixed — the loop is remembered in beats now — and the artifact outlived the fix.
- **Anything in `loadDemoContent` itself.** With no edits at all, a recording of four loop passes compared against the same music mid-loop shows a largest deviation of 0.013 against a 0.16 signal, which is oscillator phase noise.

## What could not be measured, and why

Neither artifact reproduced in a headless harness. Walking the demo forwards and undoing it — seven presses with no message loop between them, and again with 90ms between them, which is what fast clicking actually is — produced no sustained pitch: the dominant frequency alternates A4 and C4 window after window, which is the arpeggio, and the RMS holds at 0.1815 with no drift.

Measuring this class of artifact is hard for a reason that is its own issue (vhl9d0): Duet can only assert audio through an offline render, and neither artifact exists there, since the render never wraps a loop and never rebuilds a graph mid-playback. Recording the device works, but 4OSC's oscillators free-run, so two passes of the same notes are not the same samples and sample-domain subtraction bottoms out in noise at the level of the artifact itself. This issue is blocked on vhl9d0 for that reason: a level and spectrum read on the playback path is what makes these findable.

## Where to start

`te::midiPanic (Edit&, bool resetPlugins)` in `tracktion_EditUtilities.h` is the lever if the diagnosis is right. The question is when Duet should pull it — after an undo or redo that changed the project while the transport rolls is the obvious candidate, and whether that is right depends on whether the engine already sends all-notes-off on a graph rebuild and is being defeated by something Duet does. `tracktion_MidiNodeHelpers.h` does send `allNotesOff` on some paths; finding out which paths, and which one an undo takes, is the first piece of work.

## Acceptance criteria

- [ ] Both artifacts reproduce in a test, or the issue records what was tried and why they cannot.
- [ ] The cause is named — a hanging voice, or something else — with evidence rather than inference.
- [ ] Undoing while the transport rolls leaves nothing sounding that the project does not contain, however fast the presses come.
- [ ] A loop pass sounds the same whether or not an undo happened during the pass before it.
