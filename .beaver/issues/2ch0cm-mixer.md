---
id: 2ch0cm
title: Mixer
state: todo
priority: high
depends_on:
    - s1jzd4
parent: 535bbo
created: 2026-08-12T03:50:49Z
updated: 2026-08-12T03:50:49Z
---

## What to build

The Mixer tab of the bottom panel: one strip per track plus the master, each carrying a fader with its dB readout, a pan control, mute and solo, a level meter, the strip's output routing, and its insert chain. Strips take the track's name and colour, and the strip order follows the arrangement's track order.

The insert chain lists the track's plugins in order, each with a bypass and a remove, reorderable by drag, and opens a plugin's editor on double-click. Plugin editors are the only floating windows in the app; each carries the mockup's chrome — bypass, save preset, close — and closing one changes nothing about the plugin.

Meters repaint from what the engine publishes, never from a lock taken in paint, and keep up at the arrangement's full track density.

## Acceptance criteria

- [ ] One strip per track plus a master strip; strips carry the track's name and colour and reorder when the arrangement's tracks reorder.
- [ ] Fader, worked: dragging a strip's fader to −6.0 dB shows −6.0 dB in its readout, reaches the model as one Action, and undo restores the previous value; double-clicking the fader returns it to 0.0 dB as one Action.
- [ ] Pan, mute and solo edit from the strip and stay in step with the same controls on the track header — changing one updates the other immediately.
- [ ] Soloing a track silences the others during playback; unsoloing restores them.
- [ ] Meters show level during playback, sit at the floor in silence, and hold peaks briefly; painting them takes no lock and reads only engine-published values.
- [ ] The strip's output routing selects the track's destination bus and reaches the model as one Action.
- [ ] The insert chain shows the track's plugins in signal order; adding, removing and reordering are each one Action, each undoable digest-exactly, and bypassing a plugin is audible without removing it.
- [ ] Double-clicking a plugin opens its editor in a floating window with bypass, save preset and close; the window is the only floating window in the app, and closing it leaves the plugin and its parameters untouched.
- [ ] Moving a plugin's control in its editor is reflected in the model, and an automated parameter's value moves in the editor as the automation plays.
- [ ] With 60 tracks playing, meters and faders stay responsive on the software renderer.
