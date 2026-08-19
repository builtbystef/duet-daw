---
id: 4r7nlj
title: 'Complete the edit vocabulary: MIDI, automation, mixer, plugin parameters, transport'
state: in-progress
priority: high
labels:
    - needs-review
depends_on:
    - quiwf3
parent: b1j3me
created: 2026-08-11T01:50:39Z
updated: 2026-08-19T03:34:45Z
---

## What to build

The remaining op domains of the vocabulary, so that the operation set covers everything js437t's suggest vocabulary enumerates — one for one: `midi.*`; the rest of `clip.*` (setLoop, duplicate); the rest of `track.*` (create with kind midi/audio/group, setOutput); `mixer.set` (volume, pan, mute, solo) and `mixer.setSend`; `plugin.add`, `plugin.remove`, `plugin.reorder`, `plugin.setParam`, `plugin.setSidechainSource` (against engine built-in plugins — external VST3s are a later slice, aty85a); `automation.setPoints`/`removePoints`; `project.setTempo`/`setTimeSignature`. Transport control also lands here, written with no UndoManager so undo can never stop or reposition the transport. Time crosses the facade as plain seconds/beats doubles; every mutation goes through performAction.

The built-in plugins are the engine-shipped devices (decision recorded on b1j3me, 2026-08-17): the engine's 4OSC and sampler as the two instruments; its EQ, compressor, and reverb as the three effects — surfaced through the facade under those Duet-facing names.

Recording ops (arm, input, monitoring, record start/stop) are not here — they are their own slice, nfjr5x.

## Acceptance criteria

- [ ] MIDI ops: insert MIDI clip; add, remove, move, and resize notes; set velocity. Worked example: `performAction("Add note", ...)` adding a note (pitch 60, start beat 1.0, length 0.5 beats, velocity 100) is one undo step; the note reads back with those values; undo/redo are digest-exact.
- [ ] Clip ops complete: set looped state and loop length; duplicate a clip to a bar on the same or another track. Worked example: duplicate a clip to bar 5 → the copy reads back at bar 5 with its source pinned project-relative; setLoop (looped, 2 bars) reads back; each is one Action with digest-exact undo.
- [ ] Track ops complete: create a track of kind midi, audio, or group (a midi track optionally created with a built-in instrument); set a track's output to a group bus. Worked example: create a group track, route an audio track's output to it → the routing reads back; undo restores the previous routing digest-exactly.
- [ ] Mixer ops: set volume, pan, mute, and solo; set a send level to a bus. Worked example: set volume to −6.0 dB → read-back returns −6.0 dB; a send to a group bus at −12.0 dB reads back exactly; mute and solo read back; each is one Action with digest-exact undo.
- [ ] Plugin-chain ops: add a built-in plugin at a chain position, remove it, reorder it, set a parameter by value, set a sidechain source. Worked example: add a compressor to a track, set its sidechain source to another track, set a parameter → all read back; undo restores the prior chain digest-exactly.
- [ ] Automation ops: add, move, remove automation points. Worked example: a point at 2.0s with value 0.75 reads back exactly; undo restores the prior curve digest-exactly.
- [ ] Project ops: set tempo and time signature as Actions. Worked example: set 128 BPM and 6/8 → both read back exactly; undo restores 120 BPM and 4/4.
- [ ] Transport ops (play, stop, set position) go through the facade with no UndoManager: start playback, reposition, then undo an earlier edit Action — the transport keeps playing and its position is unchanged.
- [ ] Every new op is expressed exactly once, writes through the Edit's UndoManager (transport excepted), and is callable only inside performAction.
- [ ] Manual demo, recorded as a note on this issue: with the transport rolling under pw-jack, each op domain lands audibly with no glitch or xrun (prototype-verified behavior spot-checked in the product app; not an automated test).

## Notes

**claude** — 2026-08-19T02:47:42Z

Built. The vocabulary now covers every operation js437t's `suggest` tool enumerates, one for one, apart from the external-plugin half of `plugin.add`/`plugin.setParam` (aty85a) and the recording ops (nfjr5x). `duet_model` grew a second implementation file: `SessionImpl.h` holds the engine-shaped `Session::Impl`, `Session.cpp` the Session and its reads, `EditOps.cpp` the vocabulary. The public header still names no engine or JUCE type, and `duet_tests` still links the facades only, so the seam is still enforced by the build.

Nine of the ten acceptance criteria are held by tests, one per criterion, in six new suites — MidiOpsTests, ClipOpsTests, TrackOpsTests, MixerOpsTests, PluginOpsTests, AutomationOpsTests, ProjectAndTransportOpsTests. 47 tests in the suite; all four checks pass. The tenth is below.

## The vocabulary as it maps to js437t

- `midi.*` — addNote, removeNote, moveNote (pitch and beat), resizeNote, setNoteVelocity; `clip.createMidi` is insertMidiClip.
- `clip.*` — setClipLoop, duplicateClip join quiwf3's insert/move/trim/delete.
- `track.*` — createTrack (kind, name, instrument?) replaces addTrack; setTrackOutput.
- `mixer.*` — setTrackVolumeDb, setTrackPan, setTrackMute, setTrackSolo, setSend.
- `plugin.*` — addPlugin, removePlugin, reorderPlugin, setPluginParameter, setPluginSidechainSource.
- `automation.*` — setAutomationPoints, removeAutomationPoints, against an AutomationTarget of track volume, track pan, or a plugin parameter.
- `project.*` — setTempo, setTimeSignature.
- Transport — startPlayback, stopPlayback, setPlaybackPositionSeconds, on Session and not on EditOps, because it is not an edit.

New reads, because an operation needs a read-back to be testable: track(ref), notes(clip), pluginParameters(plugin), automationPoints(target), timeSignature(), barStartSeconds(bar). TrackInfo gained kind, output, the mixer values, the sends and the plugin chain. trackVolumeDb(ref) and addVolumeAutomationPoint are gone — the first is track(ref).volumeDb, the second is setAutomationPoints, and keeping either would have been a second expression of one operation.

## Decisions made in the build

1. A group bus is a track whose own output goes nowhere, and other tracks route into it with setOutputToTrack. The engine's other group shape, the submix folder track, ties routing to position in the track list, which would have made track.setOutput and moveTrack fight each other. TrackKind is read off state that means it rather than a flag Duet stores: a group is a track that outputs to none, a midi track is one with a built-in instrument in its chain.

2. Units. Positions on the timeline are seconds (matching quiwf3's clip ops), positions inside a MIDI clip and loop lengths are beats, automation points are seconds. barStartSeconds is what turns the criteria's bars into either. Automation values are in the units their target reads back in — decibels for a fader, -1..1 for pan, the parameter's own units for a plugin parameter — so a curve point and the value it drives to are written the same way.

3. Mute and solo are written straight to the track state through the undo manager. The engine binds both with no undo manager, because in its model they are monitoring controls. In Duet they are mixer.set fields a Suggestion can carry, and a Suggestion accepts as one undoable Action, so they undo like every other edit. The property is the one the engine reads, so nothing else changes.

4. The big one: a plugin's value and the state it is stored in can come apart, and the fix has to be free of side effects. The engine keeps each parameter in the plugin as well as in the state and deliberately does not follow a state change back into the plugin — a change there may be automation or a modifier speaking, and neither is the value the producer set. An undo is neither of those, so undo()/redo() now put the two back in step (refreshParametersFromState). Without it a read after an undo answers with the value the undo took away, and the fader does not move: the undo is inaudible. Two traps on the way, both now regression-tested:
   - A parameter sitting at its default has no property in the state at all, and JUCE's CachedValue::setValue always writes when it is using its default. So putting a parameter back in step CREATED a property — a state that means the same and reads differently, which the digest saw as a change, and worse, an undoable write performed after an undo, which cleared the redo stack.
   - The fix is upstream of the refresh: stateParametersExplicitly writes every parameter of a plugin into its state at the moment the plugin is made (and once over everything on open), at the value it already has, with no undo manager. A plugin that states all of its parameters from birth never gets into that position — an undo only ever puts an existing property back to an earlier value, and the refresh then writes nothing at all. It has to use the EXPLICIT value and not the current one, or a project that has just been read gets its faders stamped with wherever their curves put them at time zero (caught by 1c8sjh's save-and-reload test).

5. setSend makes the send and the return on first use. The engine routes sends by bus number and Duet routes them by track, so the number is allocated here, from the sixteen the engine names, and never surfaces. Setting the same send again sets its level rather than making a second one.

6. Looping an audio clip states its musical length first. The engine refuses to loop a source whose length in beats it does not know, and a plain wave file carries no such length; setClipLoop makes the same claim the producer makes by switching a clip to loop — the file runs for this many beats of the project's meter. MIDI clips need none of this.

7. removeAutomationPoints counts both ends in, unlike the engine's half-open range: a producer clearing a stretch of a curve means the points at its edges, and the same time at both ends means the one point there. setAutomationPoints sets the value of a point the curve already has at that time rather than stacking a second one on it.

8. A NoteRef is Duet's own handle, because the engine gives a note no durable identity. It is a note's ValueTree, kept in a map on the session: JUCE's undo of a removed child puts back the very object it took away, so a handle stays pointed at the same note across undo and redo — which the MIDI suite asserts directly.

9. loadDemoContent now goes through performAction instead of writing naked ops, since the MIDI vocabulary exists to write it with. It is still the walking skeleton's only content source and still goes away with 1c8sjh's project open.

## The manual demo — the one criterion still open

The app shell gained a Next Edit and an Undo button: one Action from each domain of the vocabulary, one to a press, against the demo phrase a new project starts with. Same status as loadDemoContent — scaffolding until 535bbo's surfaces replace it — and it exists so the demo can be listened to. To run it: `pw-jack ./build/modules/duet_app/duet_app_artefacts/Debug/Duet`, New into an empty folder, Play, then Next Edit seven times.

What is verified without ears, under pw-jack on this machine's real device: the new test "one Action from every domain lands while the transport rolls" plays a project and performs one Action per domain — notes, loop and duplicate, group routing, mixer and send, a reverb and its parameter, a volume curve, tempo and time signature — with the message loop running between them, and the transport is still rolling and the playhead still advancing after every one of them. pw-top over the same run reports ERR 0 on both of the process's audio nodes throughout; the only non-zero counter on the machine was an idle HDMI sink that Duet never touched. The app was also launched under pw-jack and its new controls confirmed on screen.

What is NOT verified: that each domain lands audibly with no glitch, which is a judgement by ear. That is the criterion's own word — "spot-checked in the product app", "not an automated test" — and it needs the user, which is why this issue is not closed.

## What the user must do

Run the demo above and listen. If each edit lands cleanly, close this issue to approve. If something clicks, drops out or stutters, write a note saying which step, and remove the needs-review label.

## For a reviewer

Session::track(ref) returns a TrackInfo by value rather than an optional — a ref that names no track reads back as a track of nothing, whose own ref is noTrack, which is how the rest of the facade already treats a ref it does not know.

**claude** — 2026-08-19T03:34:45Z

Review finding, fixed: the group bus was silent, and decision 1 of the previous note was wrong.

The user ran the demo and reported that the sound disappeared at step 3, "Route into a group bus", and stayed gone. It is a real bug and it is fixed.

## The cause

createTrack(TrackKind::group, ...) called setOutputToNone() on the new track — that was the previous note's decision 1, "a group bus is a track whose own output goes nowhere". It is exactly wrong. In the playback graph a track with no output device and no destination track is wrapped in a SinkNode, whose own comment in tracktion_TestNodes.h reads "Blocks audio and MIDI input from reaching the outputs". So the engine did process the bus — which is why its sends and returns still worked and the transport kept rolling — and then threw its audio away. Everything routed into it was silent, and so was everything sent into it, which would have taken steps 4 and 5 with it.

Measured on the real device, recording Duet's output ports into a null sink: 0.00000 RMS with the old behaviour, over the whole run. Not quiet — exactly zero.

## The fix

A group bus is now an ordinary track that the producer designated as a bus. Its own output goes to the device like any other track's, and the designation is stored as a "duetTrackKind" property on the track's own tree, written through the edit's UndoManager. On the track's tree and not in the DUET tree, so that it travels with the track: deleting the track takes the designation away and undoing the deletion brings it back, with no code of ours involved. trackKindOf reads it, and falls back to the derivation for the other two kinds — a midi track is still one with a built-in instrument in its chain — which is what lets the track the engine makes with a new edit answer without ever having been through createTrack.

Same measurement after the fix: 0.05732 RMS, peak 0.3963, against 0.18277 RMS for the same synth played on a plain track with no bus (lower RMS because the bus case is notes with gaps between them, and the plain case a continuous tone). The bus is heard.

## Why the suite did not catch it, and what now covers it

The engine builds two different graphs, and only one of them has the SinkNode. createNodeForEdit(EditPlaybackContext&, ...) is playback and adds it; createNodeForEdit(Edit&, ...) is the offline render and sums a no-output track straight into the master instead. So the render heard the bus while the producer did not. A new test, "audio routed through a group bus still reaches the output", renders a tone through a bus and asserts a non-silent peak — it pins the routing, but it passed against the broken code too, and that is the honest limit of it.

The gap is that ADR 0006 makes the offline render the instrument for every audio assertion Duet has, which leaves the playback graph unasserted by construction. Published as vhl9d0 (high, under b1j3me), with the null-sink measurement recipe in the body so it is reproducible.

## Checks

Build, format, lint and 48/48 tests all clean. The demo steps themselves are unchanged.

## What the user must do

Run the demo again from step 1 and listen through all seven. Step 3 should now sound unchanged from step 2 rather than dropping out, and steps 4 and 5 should be audible. If it is clean, close this issue to approve. If something still clicks, drops out or stutters, write a note saying which step, and remove the needs-review label.
