---
id: 4r7nlj
title: 'Complete the edit vocabulary: MIDI, automation, mixer, plugin parameters, transport'
state: done
priority: high
depends_on:
    - quiwf3
parent: b1j3me
created: 2026-08-11T01:50:39Z
updated: 2026-08-19T05:09:09Z
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

**claude** — 2026-08-19T03:54:49Z

Second review finding, fixed: the demo distorted, and undo could empty the project.

The user ran the demo again. All seven edits now land — the group bus fix holds. Three things were wrong with what came after.

## The distortion was the demo's gain staging, not undo

Reproduced and measured, playing the demo forwards through all seven steps and then undoing it press by press, recording Duet's output ports into a null sink: 5,425 samples pinned at full scale, peak exactly 1.0000, across a four-second window. Not a rebuild click — sustained digital clipping.

The cause is three of the demo's own steps stacking gain that the master cannot carry:

- Step 4 sent the track into demoBus at -12 dB — the same bus step 3 had already routed the track's output into. So the bus carried the signal twice. A send into the bus a track already outputs to is not a routing anyone would make; it is a second copy and about 6 dB nobody asked for.
- Step 5 put the reverb at the head of that bus at wet 0.6, so it wetted the whole signal rather than a send, and added its own level on top of the dry it was passing through.
- Step 6's volume curve ran to 0.0 dB at its peak, above the -6.0 dB fader step 4 had just set, so the automation undid the one gain reduction in the chain at exactly the loudest moment.

Fixed by making the send do what a send does: step 4 now creates its own "Reverb" bus and sends to that, step 5 puts the reverb there at wet 1.0 / dry 0.0 where it is a send effect, and step 6's curve peaks at -6.0 dB rather than 0.0. Same measurement after: peak 0.7747, zero clipped samples.

Why this read as an undo fault: the clipping came from the volume curve and the double routing, so undoing the tempo — the first press — changed nothing about it. It only cleared when the undo reached step 6. The undo was working; it was undoing the wrong thing to make the noise stop.

## Undo could take the demo phrase away

loadDemoContent wrote the phrase through performAction and the session had already started its undo history, so "Add the demo phrase" sat at the bottom of the stack. Enough presses and the arpeggio itself was gone and the project was empty — which is exactly what the user saw at the end. It is an undo of something the producer never did. loadDemoContent now calls startUndoHistory() after writing the phrase, so the phrase is the state the project starts from.

## Undo did not walk the demo back

The Undo button called session.undo() and left demoStep alone. The label went on naming a step that had just been taken away, and the next press of Next Edit ran the step after it against a project missing what that step needed — with demoBus and demoReverb naming deleted objects, so setSend and setPluginParameter silently did nothing. Undo now decrements the counter with the edit, which fixes the stale refs too, because each step assigns its own.

## What the model got right

Worth saying, because it was the first thing suspected: the vocabulary's undo is exact. A new test, "the vocabulary demo undoes step by step back to where it started", walks the demo's seven steps, records the digest after each, undoes back through all seven checking each digest on the way down, then redoes back up — 29 assertions, all exact. It also asserts that the eighth undo returns false and the phrase is still there, which is the regression guard for the empty-project bug.

The clipping is the second thing this review pass found that a green suite could not see, for the same reason as the first. Noted on vhl9d0.

## Checks

Build, format, lint, 49/49 tests.

## What the user must do

Run the demo once more: New, Play, Next Edit seven times, then Undo all the way back. Expected now — no distortion at any point forwards or backwards, the label tracking the Undo presses, and the arpeggio still there when the undo stack runs out. Close this issue to approve, or note what is still wrong and remove the needs-review label.

**claude** — 2026-08-19T04:32:19Z

Third review pass: the reverb was inaudible, and the demo's copy sat on the loop seam.

## The reverb was never in the signal path

setSend puts the AuxReturnPlugin at the head of the bus, because that is where a return has to be. Step 5 then added the reverb at position 0, which pushed the return behind it — so the reverb sat upstream of the only thing feeding it, processed silence, and the bus carried the send dry. The demo now passes 1.

That the right answer is a magic number is the actual fault, and it is not the demo's. addPlugin's position is a raw index over a chain that already holds plugins Duet put there and the producer never asked for: the fader every track is born with, the aux return on a bus, the aux send on the source. A Suggestion that says "put a reverb first in the chain" gets one that does nothing, silently. Published as 6i7an7 (bug, high, under b1j3me).

MixerOpsTests gains the assertion that would have caught it — "a send into a reverb bus is heard, and rings on after the source stops" — which renders a one-second stab through a send into a reverb bus and asserts the tail outlives it. It fails at position 0 (tail level 0) and passes at 1. Measuring a tail needed a windowed read, so test support gained peakLevelBetween.

## The stray note at the loop seam: what is settled and what is not

Settled, read straight off the facade: step 2 duplicated the phrase to bar 5, which is exactly where the transport loop ends — 8.0s at 120/4-4, and 6.857s after step 7, because both the clip and the loop are kept in musical time and move together. So the copy has always begun on the seam, at both tempos. The step means the copy to be out of earshot; it was on the boundary instead. It now goes to bar 9, and DemoWalkthroughTests asserts the copy starts clear of the loop's end, which is the guard that keeps it there.

Not settled: that the copy on the seam is what the user heard. Two recordings on the device, identical but for the copy's bar, folded over four loop passes and compared half against half — the phrase is two identical halves after step 2, so anything once-per-loop breaks that symmetry. Bar 5's largest asymmetry does sit at 0.06-0.18s, right after the wrap, and bar 9's sits mid-phrase. But the effect is the same size as the measurement's own noise: 4OSC's oscillators free-run, so two passes of the same notes are not the same samples, and the envelope difference floor is as large as the thing being looked for. Suggestive, not proof. The user's ear is the instrument this criterion names, and it is the one that can settle it.

## A wrong turn, recorded so it is not taken again

The first diagnosis was that the transport loop is set in seconds and stays at 8s while a tempo change moves the music underneath it. It is wrong: the loop follows the music. The test that proves it is "a tempo change carries the loop along with the music, and an undo cannot" — at 140 BPM both the phrase and the loop end at 6.857s. The app briefly grew a helper to re-set the loop after every step; it is gone, because it fixed nothing.

The transport facade did keep what that work needed and the issue's transport criterion covers: setLoopRangeSeconds, loopRangeSeconds, setLooping, isLooping, all written with no undo manager like the rest of the transport, and the test above asserts an undo cannot move any of them.

## Checks

Build, format, lint, 51/51 tests.

## What the user must do

Run the demo once more. The reverb in step 5 should now be plainly audible as a tail on the send. If the faint note at the loop seam is gone, that confirms the copy was it — say so and close the issue. If it is still there, it is something else and worth another look: say roughly where in the loop it falls and whether it is there before step 2.

**claude** — 2026-08-19T04:53:46Z

Fourth review pass: the note at the loop seam was the loop coming back short from an undo.

The reverb fix is confirmed by ear. The stray note was not the copy's position — that hypothesis was wrong, and the user's own observation is what found the real one: it appears after the first or second undo, never on the way forward. Every measurement I had run was of forward playback, which is why they all came back clean.

## What was happening

Read straight off the facade, loading the demo and setting the tempo to 140 and back:

    at load           loop=0..8s       phrase=0..8s
    after setTempo    loop=0..6.857s   phrase=0..6.857s
    after undo        loop=0..6.857s   phrase=0..8s
    after redo        loop=0..6.857s   phrase=0..6.857s

The engine rescales its own loop range when the tempo is set, and does not when that change is undone. So after undoing step 7 the transport wrapped at 6.857s while the phrase ran to 8s — a beat and a half early, at beat 13.71 of 16. The notes sit every half beat and run 0.45 of one, so the wrap landed in the middle of the note at beat 13.5: it was cut, its release rang on into the next pass, and what the producer heard was a faint tone at the start of every loop that the arpeggio's opening did not account for. The redo line looked right only because setting the tempo forwards rescales the range again.

## The fix

The loop range is remembered in beats, not seconds. The engine's range is a cached answer to a musical question, so `Session::Impl::loopBeats` holds the question and `applyLoopRange` re-answers it after anything that can move the tempo map — every Action, every undo, every redo. setLoopRangeSeconds converts on the way in and loopRangeSeconds still reads the engine, so the read is of what the transport will actually do.

This is not an undo writing the transport, which ADR 0004 forbids. Nothing goes through the UndoManager and the loop is never restored to an earlier value: it is recomputed so that it stays over the same music. The distinction has its own test, "an undo cannot switch looping off, or move a loop the tempo has not moved" — a loop at 2s..6s with no tempo change in sight is exactly where it was after an undo.

The test that asserted the old behaviour as correct is gone. It said an undo may not move the loop, which was the right principle read off the wrong evidence: I had captured the range after the tempo change and required it unchanged, which is precisely the bug. It is replaced by "the loop stays over the music through a tempo change, its undo and its redo".

## The copy at bar 9 stays

Moving step 2's copy from bar 5 to bar 9 was made against the refuted hypothesis, and it stays anyway: bar 5 is exactly where the transport wraps, the step means the copy to be out of earshot, and a copy sitting on the seam is fragile whether or not it was ever heard. DemoWalkthroughTests keeps the assertion that it lands clear of the loop's end.

## Checks

Build, format, lint, 52/52 tests.

## What the user must do

Run the demo, forwards through all seven and then all the way back with Undo. The note at the loop seam should be gone. If it is, this issue's last criterion is met — close it. If anything else is off, say so and remove the needs-review label.

**claude** — 2026-08-19T05:07:42Z

Fifth review pass: two artifacts remain, and they are not this issue's to chase further.

The user reports the reverb now works, and three things still wrong. Two are audio artifacts that survived every fix here, and one is a pre-existing fault in the app's Play button.

## Published, not fixed

- **sohgf4** — the Play button needs a second press. This is hazard 6, which AGENTS.md names and which every test survives by going through playUntilRolling: the engine rebuilds its device list once, seconds after the first playback, and that frees the playback graph and stops the transport. MainComponent calls startPlayback() once and never looks again. The button predates this issue, and the user flagged it as separate, so it is published rather than fixed. It does make this issue's demo harder to judge — a silent first press reads as a fault in whatever was just edited.

- **6629zo** — the tone at the loop seam and the one stuck by fast undos, blocked on vhl9d0. It carries the three hypotheses that were tested and refuted, the measurements that came back clean, and why a headless harness cannot see either artifact.

## The loop-seam tone: three wrong answers

Recorded plainly, because two of the three cost real time and the reasoning looked sound each time.

1. The copy on the loop seam. Step 2 duplicated the phrase to bar 5, which is exactly where the transport wraps. Moving it to bar 9 changed nothing the user could hear. The copy stays at bar 9 regardless — a clip on the seam is fragile whether or not it was ever audible.
2. The loop frozen in seconds under a moving tempo. Half right: the loop does follow the tempo forwards, and the bug was that it did not come back on the undo, leaving the transport wrapping a beat and a half early and cutting a note every pass. That was real, it is fixed, and the artifact outlived it.
3. Anything in loadDemoContent. Ruled out by measurement before it was ever proposed: four loop passes with no edits at all, compared against the same music mid-loop, deviate by 0.013 against a 0.16 signal.

The stuck tone did not reproduce either — seven undo presses with no message loop between them, and again with 90ms between them, which is what fast clicking actually is. The dominant frequency alternates A4 and C4 window after window, which is the arpeggio, and the RMS holds at 0.1815 with no drift.

The common thread in what could not be measured is vhl9d0's: every audio assertion Duet has goes through an offline render, and neither artifact exists there, because a render never wraps a loop and never rebuilds a graph mid-playback. Recording the device works but 4OSC free-runs, so two passes of the same notes are not the same samples and subtraction bottoms out in noise at the level of the artifact. Both leading suspects are hanging MIDI voices; te::midiPanic is the lever, and 6629zo says where to start.

## Where this issue stands

Nine of the ten acceptance criteria are held by tests, now 52 of them. The tenth — every op domain landing audibly with no glitch — is met for the landing and not for the glitch: all seven domains are audible and correct after four rounds of real fixes (the silent group bus, the clipping, the undo faults, the inaudible reverb, the short loop), and two tonal artifacts remain that no measurement here could pin to a cause.

That is a judgement for the user and not for me. Closing this issue and letting 6629zo carry the artifacts is defensible, since the vocabulary itself is demonstrably working and the artifacts are a distinct fault about voices outliving a graph rebuild. Holding it open until they are gone is also defensible, since the criterion says "no glitch" and there is one. Whichever, the label comes off when the user decides.

## Checks

Build, format, lint, 52/52 tests.
