---
id: 2ch0cm
title: Mixer
state: done
assignee: agent
priority: high
depends_on:
    - s1jzd4
    - aty85a
parent: 535bbo
created: 2026-08-12T03:50:49Z
updated: 2026-08-25T14:54:49Z
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

## Notes

**agent** — 2026-08-25T12:30:58Z

The contract cannot be implemented without inventing decisions at the model/UI boundary. Three specifics need resolution: (1) 'a plugin's editor' can mean the hosted VST3/native editor, which requires a new exception to the settled engine/JUCE-free duet_model facade (duet_persistence is currently the only exception), or an engine-free generic parameter editor built from pluginParameters; choose which. (2) the master strip is required to carry the same fader, pan, mute/solo, routing and insert chain as track strips, but the public model exposes only the master's output meter and no master identity or edit operations; decide which master controls exist in milestone one (and whether its routing is omitted, since external hardware routing is explicitly out of scope). (3) 'save preset' names chrome but gives no behavior or persistence contract; decide whether milestone one saves a user preset, what is named/stored, or whether this button is intentionally non-functional. Options I see are: generic parameter editor + a newly specified engine-free master facade + defined Duet preset storage; or authorize a narrow JUCE/engine editor bridge and specify the master/preset contracts around it. User must decide these points, record the decision in an issue note, and remove needs-review.

**agent** — 2026-08-25T13:07:26Z

Decision (2026-08-25, Target Producer): the Mixer contract is clarified as follows.

Plugin editors: open a VST3's native editor when available and a Duet generic parameter editor otherwise. Preserve the engine/JUCE-free public model facade; authorize a narrow component-only editor bridge and record that architectural exception in an ADR. One window may exist per plugin and several plugin windows may coexist. Reopening focuses the existing window. Removing a plugin or closing/replacing its project closes the window; undo never reopens it. Closing an editor changes no plugin state. Native parameter gestures are transiently audible and commit as one Action at gesture-end (mouse boundaries are the fallback when a plugin omits gesture boundaries); automation-originated movement updates the editor without creating Actions.

Master: use a specialized neutral Master strip, not a fake arrangement track. It has fader/readout, pan, mute, published output meter, inserts, and a fixed read-only Main Output. It has no Solo, editable routing, or track colour. Give it an engine-free model read and stable opaque identity usable by mixer/plugin operations where meaningful; arrangement track enumeration remains unchanged.

Bypass: persisted project state, shared immediately between chain and editor, audible, and one Action per toggle, with exact undo. It is not automation-controlled in milestone one and is excluded from presets.

Presets: implement a complete app-global Duet preset library keyed by stable plugin identity, storing format version, identity, trimmed producer name, and opaque plugin state. The editor has a sorted selector plus Save Preset. Save/replace does not dirty the project or enter undo; names are non-empty and case-insensitively unique, and replacement asks Replace/Cancel. Loading is one "Load Plugin Preset" Action with exact undo; incompatible/unreadable data changes nothing; parameter edits show Custom. Rename/delete are deferred.

Mixer gestures: fader range is -60..+6 dB with a bottom -infinity detent represented by silentDb; double-click resets to 0 dB. Pan is -1..+1 and double-click centres it. Dragging fader/pan updates a transient audible value and the readout without dirtying/undo; mouse-up reverts the transient and commits the final value as exactly one Action; cancellation restores the original with none.

Routing/inserts: ordinary tracks offer Main Output and cycle-safe group destinations; groups offer Main Output and other non-cycling groups; selecting the current destination does nothing. Master output is fixed. Each chain ends in + Insert, grouped into compatible built-in instruments/effects and available VST3 instruments/effects; instruments are offered only on MIDI tracks, effects also on Master. Browser drag later must call the same paintless insertion operation. Remove is immediate and closes an open editor; undo is recovery. Reorder previews an insertion marker and commits once on a valid drop; Escape/outside/current-position drops emit no Action.

Meters/performance: sample published peaks at 30 Hz on the message thread, hold the maximum for 1 second, then fall at 24 dB/s to silentDb; paint reads cached scalars only and takes no lock. Sample/repaint visible strips only, repaint only affected regions during gestures, and rebuild structural metadata only when structure changes. Deterministic tests cover 60-track state, visible-range sampling, peak hold and Action behavior; final validation runs 60 playing tracks on the software renderer and records fluidity, with no flaky wall-clock CTest or paint test.

These decisions supersede the literal implication that every control on an ordinary track also appears on Master. The Target Producer confirmed this is the shared understanding and approved removing needs-review.

**agent** — 2026-08-25T14:54:49Z

Completed the Mixer slice. Added the paintless Mixer seam and live MixerCanvas with ordered coloured track strips plus specialised Master, transient fader/pan gestures and resets, shared header pan/mute/solo Actions, cycle-safe routing, compatible built-in/VST3 insertion, bypass/remove/reorder, cached 30 Hz visible-only meters with one-second hold and 24 dB/s fall, and revision-cached structural state. Added the stable engine-free Master identity/read and Master mixer/plugin vocabulary. Added native-VST3/generic editor windows (one per plugin, focus-on-reopen, close on removal/project replacement), gesture-to-Action translation with mouse fallback, shared bypass, and an app-global versioned Duet preset library with exact Action-based loads. ADR 0008 records the narrow component-only editor bridge; architecture docs are updated. Tests cover gesture Action counts/cancellation, exact undo, routing cycles, solo audibility, bypass audibility, opaque preset load/undo, Master, 60-track visible metering/hold, preset uniqueness/sorting, and the live GUI surface. Software-renderer validation used a saved 60-track playing project: the app accepted Mixer and Play keys and stayed live for the ten-second run (46.1% CPU, 0.7% memory, no stall observed). That run also exposed the pre-existing Piano Roll UTF-8 Debug assertion, published separately as bdj09t; it is unrelated and non-blocking. Checks: clang-format-18 passed; full lint passed; full Debug build passed with -j 4; ctest passed all 250 tests with the 8 expected hardware skips. No criteria remain open.
