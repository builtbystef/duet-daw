# Engine notes

What Tracktion Engine actually does, as Duet has already paid to learn it. The
rules: one fact per entry. Each entry states what the engine does, where in the
engine it does it, how the fact was proved, and what Duet does about it. Hazard
numbers are stable — code comments cite them by number; never renumber a hazard
and never reuse a retired number. New facts go at the end of the right section.
Do not copy these facts into a spec or an issue note; point here.

A **hazard** is an engine behaviour Duet must work around, numbered because
comments already cite it. The further facts are the same kind of record without
a number comments depend on.

## Hazards

### 1. Naked ops outside an Action

**The engine.** An op written while no named transaction is open merges into the
previous transaction, or lands in an unnamed step after the engine's
`UndoTransactionTimer` seals.

**Where.** The Edit's `UndoManager` and its 350 ms timer. The timer fires when
no mouse is down and the message loop has been quiet for 350 ms.
`Edit::UndoTransactionInhibitor` suspends it.

**Proved.** `skb4tp` finding F1, on `prototype/undo-vocabulary`.

**Duet.** Every project change goes through `Session::performAction`, the only
transaction boundary (ADR 0004). Raw ops are private to it. `performAction`
holds an `UndoTransactionInhibitor` so a long Action is not split in two.

### 2. The async clip re-sort is undo-tracked

**The engine.** After a clip move or trim, the engine schedules an asynchronous
clip re-sort that writes through the Edit's `UndoManager`. If that write lands
after an undo, it clears the redo stack.

**Where.** `ClipOwner` (`ValueTree::sort` with the Edit's undo manager).

**Proved.** `skb4tp` finding F2, on `prototype/undo-vocabulary`.

**Duet.** Actions stay open: `performAction` and `stopRecording` begin a
transaction and deliberately do not seal it, so the re-sort joins the Action
that caused it. The engine itself suppresses the sort during undo and redo.

### 3. `Edit::flushState()` writes parameter blobs through the UndoManager

**The engine.** `Edit::flushState()` flushes every plugin unconditionally
(`tracktion_Edit.cpp:1176`). The blob write
(`AutomatableEditItem::saveChangedParametersToState`) goes through the
UndoManager exactly when a parameter's `currentValue != currentExplicitValue` —
so a flush is undo-neutral until automation has driven the parameter, and after
that it seals an unnamed transaction and destroys the redo stack. Tracktion's
own save path has this flaw.

**Where.** `Edit::flushState`; `AutomatableEditItem::saveChangedParametersToState`.

**Proved.** `skb4tp` finding F3; characterised precisely by `rquzdc` on
`prototype/duet-persistence` (experiment E3).

**Duet.** The save is a snapshot, never a flush (ADR 0005): `Project::save`
copies `edit.state`, writes the same blobs onto the copy with a null
UndoManager, and writes the copy. `Session::stateDigest` also copies and does
not flush, so asking what the state is does not change it.

### 4. `EditFileOperations::save` segfaults for a project-less edit

**The engine.** `EditFileOperations::save` crashes on an edit that has no
ProjectManager item. `EditSnapshot::refresh` null-dereferences the
`ProjectItem`. Upstream bug.

**Where.** `EditSnapshot::refresh` (`tracktion_EditSnapshot.cpp`).

**Proved.** `ddp1qt`, reproduced three times and confirmed in gdb; recorded on
`skb4tp`.

**Duet.** The save never uses `EditFileOperations::save`. It writes the
snapshot XML itself, beside the project file, and renames it on.

### 5. `insertWaveClip` stores a source the engine cannot read back

**The engine.** `insertWaveClip` writes the clip's source with
`PathStyle::chooseBest`, which is `getRelativePathFrom(editFile)` — relative to
the edit *file*. The read is
`getEditFileFromProjectManager(edit).getChildFile(source)`, relative to the
folder *holding* that file. One level apart. The stored path names a file that
does not exist, and the clip plays silence. The same write happens for a clip
the engine inserts as a recording lands.

**Where.** `AudioTrack::insertWaveClip` / `SourceFileReference` on the write;
`getEditFileFromProjectManager` on the read.

**Proved.** `ddp1qt`; stated precisely by `quiwf3` note 3 and again on the
record path by `nfjr5x` note 3. Discriminating: commenting the pin out fails
the assertion that the stored reference starts with `audio/`.

**Duet.** Every insertion pins the reference the project reads —
project-relative for a file inside the project folder, absolute for one
outside — via `projectReferenceTo`. `stopRecording` pins every clip the take
just made the same way. `Edit::alwaysUseRelativePaths` is deliberately not
set: it only makes the engine produce more of the off-by-one paths.

### 6. The engine rebuilds its device list and frees every playback graph

**The engine.** `DeviceManager::initialise` sets a four-second timer that calls
`DeviceManager::applyNewMidiDeviceList`. The rebuild lands four seconds into a
session, not into its first playback. It clears and reloads every playback
context's devices, which frees the graph and stops the transport. It is not
one event: `checkDefaultDevicesAreValid` then settles the defaults
(`setDefaultMidiOutDevice` → `rescanMidiDeviceList`) and schedules a second
rebuild about five milliseconds behind the first. A take started between the
two is ended by the second, and reads as a rolling transport that produces no
clip. The only sign the engine offers that the build has happened is a
non-empty MIDI input list (before it, none; after it, at least the engine's
own "All MIDI Ins").

**Where.** `DeviceManager::initialise` (the four-second timer);
`DeviceManager::applyNewMidiDeviceList`; `checkDefaultDevicesAreValid`.

**Proved.** `skb4tp` finding F5 (the transport dying ~3 s after first
playback, originally mistaken for an undo bug). Measured on the record path
by `di0frj`: a take started as the first gesture of a session stopped at
t = 4.0 s, and at that instant the session's input count went from 33 to 35.

**Duet.** Two remedies, because asking again is safe for playback and fatal
for a take.

- **Playback** (`sohgf4`). `Session::startPlayback` remembers that playback
  was asked for and, on a 100 ms timer, asks a transport that is not rolling
  to play again. Bounded at 100 asks (ten seconds); the counter resets every
  tick that finds the transport rolling, so the rebuild — which arrives after
  playback has started — gets the whole window again. `stopPlayback` stops
  the timer before it stops the transport. The asking lives in the model so
  every caller of `startPlayback` is covered, not only the tests.
- **Recording** (`di0frj`). `TransportControl::record` starts a take at the
  playhead, so a second ask would land the first half as one clip and begin
  another. That is why `Session::startRecording` waits. It asks whether the
  devices are settled — MIDI input list built, and unchanged for 100 ms since
  the last device-change broadcast. If they are, the take starts now. If they
  are not, the session asks the engine for the build immediately
  (`rescanMidiDeviceList`) rather than four seconds from now, and a 20 ms
  timer starts the take once the churn stops. Bounded at two seconds, after
  which the take starts regardless. In an open session the wait is zero. A
  take may begin slightly after `startRecording` returns; `isRecording` says
  which. Stop and Play both cancel a take that has not begun.

Headless tests no longer retry `play()` until `isPlaying`. They start
playback once and pump the message loop, because the asking is the model's.

Tests drive the rebuild at the device seam (`2jqmj2`): `suppressDeviceRebuild`
stops the engine's four-second timer, `rebuildDevices` asks for the list now
and frees the playback graph, and `setDeviceWait` makes the pre-roll's quiet,
poll and bound drivable so those cases spend no real seconds. The ask is not
on the constructor path.

### 7. Undo and redo permute ValueTree property order

**The engine.** An undo/redo round trip re-appends restored properties, so
their order on the `ValueTree` is not stable.

**Where.** JUCE's `UndoManager` property restore.

**Proved.** `skb4tp` finding F4.

**Duet.** Every state comparison goes through `Session::stateDigest`, which
canonicalizes: properties and children are written in a stable order, empty
nodes are dropped, and engine-owned noise (`projectID`, the `TRANSPORT` node)
is stripped. Tests never compare raw XML.

### 8. A full-parallel Tracktion build will OOM the dev machine

**The engine.** Each Tracktion translation unit is about 2 GB to compile.

**Where.** The vendored Tracktion tree, compiled as part of this project.

**Proved.** `skb4tp` finding F6, on the 15 GB / 12-core dev machine.

**Duet.** Local builds are `-j 4`. The command is in `AGENTS.md`. CI is a
different machine and is not bound by this.

## Further facts

### `Edit::undo()` stops a running recording

**The engine.** `Edit::undoOrRedo` opens with
`if (getTransport().isRecording()) getTransport().stop (false, false, true);`
— it stops a running take before it reverts anything, by policy. Writing
transport properties with a null UndoManager does not change that.

**Where.** `Edit::undoOrRedo`.

**Proved.** `nfjr5x` note 1. The test *an undo during a take neither stops it
nor moves the playhead* failed on the `Edit::undo()` routing and passed on
the project's `UndoManager`.

**Duet.** `Session::undo()` and `redo()` run the project's `UndoManager`
directly. That is why undo does not go through `Edit::undo()`. Nothing else
`Edit::undo()` does applies: the rest refreshes the engine's
`SelectionManager`s, and Duet registers none. Spec `b1j3me` still names
`Edit::undo()`; ADR 0004 does not. The mechanism sentence in the spec is
what this serves.

### `EngineBehaviour::getFileForNewAudioRecording` is the hook for take paths

**The engine.** Left to itself, the engine writes a new audio recording into
the directory its filename pattern names, which is not the project folder.

**Where.** `EngineBehaviour::getFileForNewAudioRecording`.

**Proved.** `nfjr5x`.

**Duet.** The one `EngineBehaviour` Duet supplies (`RecordingBehaviour`)
implements that hook. `Project` tells the session the folder shape it owns
(`Session::setRecordingDirectory` with `audioDirectory`); a session nobody
has told writes takes beside its edit file.

### `HostedAudioDeviceInterface` is the device seam

**The engine.** `HostedAudioDeviceInterface` is what a caller drives when
there is no audio hardware: the device manager is switched to it, and blocks
are pushed in. The engine's own headless test player
(`tracktion_EnginePlayer.h`) uses it.

**Where.** `DeviceManager::getHostedAudioDeviceInterface`;
`tracktion_EnginePlayer.h`.

**Proved.** `vhl9d0`.

**Duet.** `Session::useNoAudioDevice` (and `playWithoutAudioDevice` /
`runWithoutAudioDevice`, which call it) switch the device manager to the
hosted device and hand it blocks. That is how playback meters and recording
run in CI (ADR 0006).

### A hosted-device switch leaves one MIDI apply pending

**The engine.** Initialising `HostedAudioDeviceInterface` applies its MIDI
list synchronously, but settling the default devices schedules another MIDI
apply on the `DeviceManager` timer. That apply reloads the playback context's
devices and frees its graph. Blocks can be pushed through a recording before
the message loop delivers the apply, so delivering it afterwards ends the take.
Hosted MIDI itself cannot change when `Parameters::useMidiDevices` is false.

**Where.** `HostedAudioDeviceInterface::initialise`;
`DeviceManager::applyNewMidiDeviceList`; `checkDefaultDevicesAreValid`.

**Proved.** `wdt64u`. The headless undo-during-take case pushed blocks, then
pumped the message loop to read the playhead; recording changed from true to
false when the pending MIDI apply landed. Cancelling that scan left recording
true through the playhead advance and undo.

**Duet.** `Session::useNoAudioDevice` cancels MIDI scanning for that session
after the hosted list has been applied, while restoring the production scan
interval in `PropertyStorage` for later engines. A headless take therefore runs
until `stopRecording` rather than until the next message-loop pump.

### The engine builds two different graphs

**The engine.** `createNodeForEdit(EditPlaybackContext&, ...)` is playback.
It wraps a track with no output and no destination in a `SinkNode` that
blocks its audio, so a group bus in that state is silent at the output.
`createNodeForEdit(Edit&, ...)` is the offline render. It sums the same
track into the master, so the same bus is audible in a render. A measured
offline render therefore cannot answer whether a producer hears anything.

**Where.** `EditNodeBuilder`: the two `createNodeForEdit` overloads; the
`SinkNode` on the playback path; the `LevelMeasuringNode` wrapped around the
default wave output after the master plugins (playback only).

**Proved.** `vhl9d0`. Red proof: a group created with `setOutputToNone` read
track −8.09 dB, bus −8.09 dB, **output −100 dB** on the playback graph. The
same project rendered audible. With the none-output removed: output −11.09 dB.

**Duet.** Audibility and headroom are asserted on the playback graph, through
`Session::outputPeakDb` / `trackPeakDb` fed by `playWithoutAudioDevice`,
which runs that graph — `SinkNode` and all — with no audio hardware (ADR 0006
amendment). A group bus is created with a real output.

### A recorded clip lands through the Edit's UndoManager

**The engine.** Every clip the engine writes as a recording lands goes through
the Edit's own `UndoManager`: `ClipOwner`'s
`addChild (clipState, -1, &edit.getUndoManager())`.

**Where.** `ClipOwner::addChild`.

**Proved.** `nfjr5x` note 2.

**Duet.** `stopRecording` opens a `beginNewTransaction ("Record Take")`,
holds the transaction inhibitor, stops the transport, and does not seal. The
deferred clip re-sort (hazard 2) merges in, so one open transaction collects
the whole take.

### `Destination::recordEnabled` is written with a null UndoManager

**The engine.** `Destination::recordEnabled` refers to its property with a
null UndoManager, so an undo can never disarm a track mid-take. The armed
flag and the input assignment still live in the Edit's `INPUTDEVICES` state
and travel with a save.

**Where.** `Destination::recordEnabled`.

**Proved.** `nfjr5x` note 5.

**Duet.** Arming and input choice are written with no undo history, the same
way. They say where the next take comes from, which is not something the
project holds.

### Input instances need a playback context; reads do not

**The engine.** Input *instances* are handed out only through an allocated
playback context, so a setter that talks to an instance must allocate one.
The Edit's `INPUTDEVICES` state can be read without that.

**Where.** `EditPlaybackContext::getInputFor`; the Edit's `INPUTDEVICES`
child.

**Proved.** `nfjr5x` note 6.

**Duet.** Setters (`setTrackInput`, `setTrackRecordArmed`) allocate a
playback context. `TrackInfo::input` and `recordArmed` are read straight out
of `INPUTDEVICES`, so asking a question about a track does not open the
machine's audio hardware.

### Where the engine's own plugins sit in a track's chain

**The engine.** A track the engine makes is born with a `VolumeAndPanPlugin`
and a `LevelMeterPlugin`, in that order, and they stay at the end of the
chain. `AuxReturnPlugin` and `AuxSendPlugin` are ordinary chain members with
no reserved place of their own — where they land is whatever index the caller
passes.

**Where.** `PluginList::addDefaultTrackPlugins`, called for each new track;
`PluginList::insertPlugin`.

**Proved.** `6i7an7`, with `tests/scratch`. A bus that a send had been made
into and one equaliser added to read back `Aux Return #1`, `4-Band
Equaliser`, `Volume & Pan Plugin`, `Level Meter`; the source track read
`Volume & Pan Plugin`, `Level Meter`, `Aux Send #1`.

**Duet.** `setSend` inserts the return at the head of the bus and appends the
send, which puts the producer's plugins in one stretch between the return and
the fader. A chain position in the vocabulary counts inside that stretch and
is clamped to it (`rawPositionFor`, ADR 0007), so a plugin can never be put
in front of the return that feeds it.

### The transport's position stands still without a real device

**The engine.** While blocks are pushed through the hosted audio device
interface, `TransportControl::isPlaying()` is true, the graph runs and the
meters read, but `getPosition()` stays where it was. The position advances only
while a real device is open and the message loop runs.

**Where.** `TransportControl::getPosition()` reads the position the playhead
publishes; the publishing happens on the message thread's own timer, which is
not running while blocks are being pushed.

**Proved.** `5he6vd`, with `tests/scratch`. Playing four half-second stretches
through the hosted interface left the position at 0.0 s every time. The same
edit on the ALSA device also read 0.0 s through the first `pumpMessages` call,
however long that call ran, and advanced from the second one on — 0.16 s, 0.37 s,
0.57 s over successive 200 ms pumps.

**Duet.** What playback puts out is asserted with no device (ADR 0006), but a
test about the playhead *moving* needs one, and skips where there is none. It
also has to pump until the position moves rather than pump once for long enough:
`ArrangementViewTests` does both.

### An undo can leave a MIDI plugin voice sounding after its note is gone

**The engine.** When an undo or redo removes a MIDI note that is already
sounding, the playback graph can leave the plugin voice running. The project
has neither the note nor the note-off that would have ended it, so the voice
survives later blocks and loop wraps indefinitely.

**Where.** The replacement `LoopingMidiNode` takes the old node's
`ActiveNoteList` in `prepareToPlay`; the voice itself stays in the instrument
plugin. `midiPanic` reaches each plugin directly and ends its MIDI voices.

**Proved.** `6629zo`, on the playback path through the hosted-device seam. An
undo removed the only note from the facade read while the output kept peaking
around -11.5 dB; the same voice crossed a loop wrap and read -13.0 dB in a
stretch with no note. Calling `midiPanic` made both stretches exactly silent at
-100 dB. Seven undo presses with no block between them reproduced the same
hanging voice before the remedy.

**Duet.** After a successful undo or redo while the transport is rolling,
`Session` calls `midiPanic (edit, false)`. This ends MIDI voices without
resetting plugins, so effect tails are not discarded.
