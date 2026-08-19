---
id: nfjr5x
title: 'Recording: record-arm, input selection, monitoring, and a take landing as one Action'
state: in-progress
priority: high
labels:
    - needs-review
depends_on:
    - quiwf3
    - 1c8sjh
parent: b1j3me
created: 2026-08-17T04:09:52Z
updated: 2026-08-19T07:19:26Z
---

## What to build

The record path through the engine-free vocabulary, per spec b1j3me's thread model and kimula's milestone scope (single-take recording; comping, punch-in, and loop-recording stay milestone two). Ops: record-arm a track, select its input (an audio device channel or a MIDI input), and set input monitoring. Transport record start/stop drives the engine's record path, written with no UndoManager like the rest of transport. A finished take lands as exactly one Action ("Record Take"): the new clip appears with its source written under the project folder's audio/ subdirectory and pinned project-relative, so a single undo removes the take's clip (the audio file may remain on disk). Latency compensation is the engine's own — this slice verifies the recorded clip lands where the engine's compensation puts it, and reimplements nothing.

This slice owns the vocabulary and engine wiring only. The transport-bar UI (record button, countdown into an untitled project) is 1fumn6's; the header arm toggle is s1jzd4's — both drive the ops built here.

## Acceptance criteria

- [ ] Arm, input-selection, and monitoring ops exist on the facade; armed state and the chosen input read back; the facade's public headers stay free of engine and JUCE include paths.
- [ ] Record start/stop go through the facade with no UndoManager: undoing an earlier edit Action during recording neither stops the transport nor moves its position.
- [ ] A recorded MIDI take (driven headlessly through the engine's record path) produces a clip whose notes read back; the take lands as exactly one Action named "Record Take"; a single undo removes the clip digest-exactly and redo restores it.
- [ ] A recorded audio take writes its file under the project folder's audio/ subdirectory with a project-relative source reference and lands as one Action; undo removes the clip and does not delete the file. If the engine's audio-record path cannot be driven headlessly, this criterion moves to the manual check below and the automated coverage is the MIDI take.
- [ ] Recording into a brand-new untitled project works without any prior explicit save — the project is a real folder from the start (ADR 0005; 535bbo's lifecycle relies on this).
- [ ] Manual check, recorded as a note on this issue: a live take on the dev machine via pw-jack, with monitoring audible while recording (not an automated test).

## Notes

**claude** — 2026-08-19T07:19:26Z

Built, and five of the six acceptance criteria are met and covered by tests. The sixth is the live take, which needs the producer's ears — see **What is left** at the bottom. 63 tests green; format, lint and the suite all clean.

## What is there

`duet_model` grew the record path, in a new `src/Recording.cpp`:

- **Inputs.** `Session::availableInputs()` returns `InputInfo` — an opaque `InputRef`, a name, and `InputKind::audio` or `midi`. `setTrackInput` / `setTrackRecordArmed` assign and arm; `TrackInfo::input` and `TrackInfo::recordArmed` read back. `setInputMonitoring` / `inputMonitoring` carry `InputMonitoring::off | whileArmed | on`.
- **Transport.** `startRecording` / `stopRecording` / `isRecording`, all written with no undo history, like the rest of the transport. `stopPlayback` delegates to `stopRecording` when a take is rolling, so a take is landed the way a take has to be landed whoever asked.
- **Running with no audio device.** `useNoAudioDevice()` (what `playWithoutAudioDevice` used to do for itself) and `runWithoutAudioDevice (seconds, InputSignal)`, which pushes blocks while the transport does whatever it is doing and plays notes and a tone into the inputs. This is what puts recording in CI (ADR 0006), and it is what makes both takes below automated rather than manual.
- **Where a take is written.** Duet now hands the engine an `EngineBehaviour` — its first — whose only job is `getFileForNewAudioRecording`.

`tests/RecordingTests.cpp`, six cases, one per criterion plus the monitoring read-back.

## Decisions

**1. Undo no longer goes through `Edit::undo()`, and this is a deliberate deviation from the spec's wording.** Spec b1j3me says undo/redo route through `Edit::undo()/redo()`, and gives the reason in the same sentence: transport properties are written with a null UndoManager *so that undo can never stop or reposition the transport*. Those two cannot both hold. `Edit::undoOrRedo` opens with

    if (getTransport().isRecording())
        getTransport().stop (false, false, true);

— the engine stops a running take before it reverts anything, by policy, and no amount of null-UndoManager writing changes that. This slice's criterion states the intent unambiguously ("undoing an earlier edit Action during recording neither stops the transport nor moves its position"), so `Session::undo()/redo()` now run the project's `UndoManager` directly. Nothing is lost: the rest of `Edit::undoOrRedo` refreshes the engine's `SelectionManager`s, and Duet registers none — the GUI area's surfaces are paintless view-models (ARCHITECTURE.md), not engine selections. The test that pins this is *an undo during a take neither stops it nor moves the playhead*, and it failed on the old routing. ADR 0004 itself says nothing about `Edit::undo()`, so no decision is being reversed — only a mechanism sentence in the spec body, whose stated purpose this change serves. **A reviewer should confirm this reading; if the spec meant `Edit::undo()` literally, criterion 2 cannot be met and one of the two has to give.**

**2. The take is one Action opened at the stop.** Every clip the engine writes as a recording lands goes through the Edit's own `UndoManager` (`ClipOwner`'s `addChild (clipState, -1, &edit.getUndoManager())`), so `stopRecording` opens a `beginNewTransaction ("Record Take")`, holds the transaction inhibitor, stops the transport, and — as everywhere — does not seal. The deferred clip re-sort merges in, exactly as it does for `performAction`.

**3. Hazard 5 applies to the record path too, and had to be handled again.** `insertWaveClip` writes a recorded clip's source with `PathStyle::chooseBest`, which is `getRelativePathFrom(editFile)` — relative to the edit *file* — while the read is `getEditFileFromProjectManager(edit).getChildFile(source)`, relative to the folder *holding* it. One level apart. So `stopRecording` takes the recording file of each armed target before the stop, notes which clips existed before it, and pins the reference of every clip the take made. Verified discriminating: commenting the pin out fails the assertion that the stored reference starts with `audio/`.

**4. Which directory a take goes into is persistence's to say, not the model's.** The model knows which folder a project is; the *shape* of that folder is `duet_persistence`'s (ADR 0005, and 1c8sjh moved the edit file out of the model for exactly this reason). So `Session::setRecordingDirectory` exists and `Project`'s constructor calls it with `audioDirectory (folder)`. Hardcoding `"audio"` in the model would have been a second copy of a name persistence owns. A session nobody has told writes takes beside its edit file.

**5. Arming and input choice are written with no undo history.** They say where the next take comes from, which is not something the project holds — and the engine agrees: `Destination::recordEnabled` refers to its property with a null UndoManager. It follows that an undo can never disarm a track mid-take. They do live in the Edit's own `INPUTDEVICES` state and so travel with a save.

**6. Reads do not open an audio device.** The engine hands out input *instances* only through an allocated playback context, so the setters allocate one — but `TrackInfo::input` and `recordArmed` are read straight out of the Edit's `INPUTDEVICES` state, because a question about a track should not be what opens the machine's audio hardware.

**7. `startRecording` asks once, unlike `startPlayback`.** Asking again would not continue a take — `TransportControl::record` starts one at the playhead — so a retry would land a partial take as one clip and start another. Published as its own issue: **di0frj**, *A take does not survive the engine's one-time device rebuild*. The headless path never sees it (a session with no audio device has no device list to rebuild), which is also why no test in this slice catches it.

**8. `duet_app` grew throwaway record chrome** — an input list, a monitoring list, Arm and Record — in the same spirit as 1c8sjh's New/Open/Save. It exists so the live take can be done at all, and ce17ym / 1fumn6 / s1jzd4 replace it.

## Facts a reviewer needs

- **The wave inputs with no audio device are two mono devices**, "Input 1" and "Input 2", plus two MIDI inputs ("MIDI Input", the hosted one, and "All MIDI Ins"). `runWithoutAudioDevice` writes its tone into both audio input channels, so which one a test picks does not matter.
- **`playbackPositionSeconds()` lags the graph, by design of the engine.** The transport publishes the playhead from a message-loop timer, and it refuses to for 200 ms after anything sets the position. With no audio device the blocks and the message loop do not run at the same time, so the test that needs a moved playhead runs the take and pumps until there is one to read. In the app the loop always runs and this is invisible.
- **`useNoAudioDevice` now takes two input channels where `playWithoutAudioDevice` used to take none.** Every existing playback and meter test still passes unchanged.
- **The audio take is asserted to land at the playhead** (started deliberately at 4 s, not 0), which is how this slice says it reimplements no latency compensation: whatever the engine's compensation decides, Duet moves the clip nowhere.

## What is left — the manual check (criterion 6)

Not done, and it is the one criterion that cannot be automated: it asks for a live take on the dev machine with **monitoring audible while recording**, and audible is a judgement I cannot make. The app builds and launches (`pw-jack ./build/modules/duet_app/duet_app_artefacts/Debug/Duet`, confirmed running), and it now has the controls to do it, but this machine has no way for me to drive a JUCE window — no `xdotool`, and `xwd` fails on this display — so the take itself needs the producer.

**To finish it:** New a project in `~/Music/…` → Add Track (this is the track that arms; it is the last one) → pick an input from the first list → leave the second list on *Monitor: while armed* → Arm → Record, play something into the input, then Stop. Expected: the input is audible while the take rolls, the transport line reads *Recording*, and Stop leaves one clip whose file is in the project's `audio/` subdirectory, with a single Undo taking the clip away and the file staying on disk.

Then either close this issue, or note what went wrong and take `needs-review` off. Labelled `needs-review` and unassigned in the meantime.
