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
on the constructor path. `rebuildDevices` waits for both applies rather than
counting out a stretch of wall clock (`20u1dr`): it returns once the MIDI
input list is built and the engine has said nothing about its devices for
50 ms, and gives up after two seconds, still under the engine's own timer.

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

### External parameters are not project state until a flush

**The engine.** An `ExternalAutomatableParameter` writes the hosted instance and
marks itself changed, but is not attached to a `CachedValue`; its explicit value
reaches the plugin node only when `ExternalPlugin::flushPluginStateToValueTree`
writes the changed-parameter blob. A direct parameter change therefore creates
no undoable state of its own.

**Where.** `ExternalAutomatableParameter::parameterChanged`;
`AutomatableParameter::setParameterValue`; `ExternalPlugin::buildParameterList`
and `flushPluginStateToValueTree`.

**Proved.** `aty85a`. A VST3 parameter changed and read back from the instance,
but opened no named undo transaction until Duet stated the value separately.

**Duet.** Each external plugin carries a `DUET_EXTERNAL_PARAMETERS` child. It is
stated when the plugin is inserted, changed through the Action's UndoManager,
and applied to the hosted instance after undo, redo, and project open.

### A VST3's opaque state belongs on the save snapshot

**The engine.** `ExternalPlugin::flushPluginStateToValueTree` asks the hosted
instance for its opaque state and writes it through the Edit's UndoManager, so
using that flush to prepare a save would mutate project state and undo history.

**Where.** `ExternalPlugin::flushPluginStateToValueTree`.

**Proved.** `aty85a`, by source reading and the purpose-built VST3 fixture's
save/reload test.

**Duet.** The persistence snapshot asks the instance for its state while
processing is suspended and writes the base64 blob only onto the copied plugin
node. The live Edit and its redo stack are untouched.

### `toBitSet` answers with every track whatever it is asked about

**The engine.** `toBitSet (const juce::Array<Track*>&)` looks up the edit of the
first track it is given and then iterates `getAllTracks (edit)` rather than the
array it was passed, setting a bit for each. The argument decides only which
edit is read. Every caller therefore renders the whole edit.

**Where.** `toBitSet` in `tracktion_EditUtilities.cpp`; the bitset it returns is
`Renderer::Parameters::tracksToDo`.

**Proved.** `6zog6s`, by source reading and then by the per-track render: a
bitset built by hand for one track of a two-track edit renders that track's tone
and nothing of the other's, and the same render through `toBitSet` holds both.

**Duet.** `Session::renderTrackToFile` sets the bit itself, at the track's index
in `getAllTracks`. `renderToFile` sets the whole range, which is the only thing
`toBitSet` was ever right for.

### A render's setup and teardown are the message thread's, its blocks are not

**The engine.** `Edit::ScopedRenderStatus` asserts the message thread and frees
the playback context; `Renderer::RenderTask`'s constructor and its first
`runJob` both reach back to the message thread through `callBlocking` to build
the graph and the render context. The block loop in between is ordinary worker
work, and `callBlocking` short-circuits when it is already on the message
thread.

**Where.** `Edit::ScopedRenderStatus`; `render_utils::createRenderTask`;
`Renderer::RenderTask::renderAudio`.

**Proved.** `6zog6s`, with `tests/scratch`. A render driven entirely from a
worker thread asserted at `tracktion_Edit.cpp:796` on every call; marshalling
the guards through `callBlocking` left it silent, and the same probe with the
message loop stopped never returned.

**Duet.** `renderTracksToFile` puts the guards up and takes them down inside
`callBlockingCatching`, so an offline render can be called from a worker thread
— which is where the thread model puts one — as long as the message loop is
running. The test harness runs every render that way (`duet::testing::Render`).

### Every rendered file carries the wall clock in its header

**The engine.** The render's writer adds broadcast-wave metadata built from
`juce::Time::getCurrentTime()`, so the origination date and time of the render
land in the file's `bext` chunk.

**Where.** `AudioFileUtils::addBWAVStartToMetadata`;
`juce::WavAudioFormat::createBWAVMetadata`.

**Proved.** `6zog6s`. Two renders of one edit a second apart differed in exactly
one byte, at offset 441 — inside the `bext` chunk — and in none of the 706 000
that follow; two renders inside the same second were identical throughout.

**Duet.** The determinism canary compares the samples and not the files
(`Render::isBitIdenticalTo`). ADR 0006's "byte-identical output" is the audio;
the header is a timestamp and says nothing about the render.

### Null ValueTree writes can provoke undo-tracked engine bookkeeping

**The engine.** A complete project-state change written directly to the Edit's
ValueTree with a null UndoManager can make an engine listener answer with its
own write through the Edit's UndoManager. That answer opens an unnamed undo
transaction and stashes the existing redo future even though the initiating
write was deliberately non-undoable.

**Where.** Edit and track listeners responding to ValueTree mutations; JUCE's
`UndoManager::moveFutureTransactionsToStash`.

**Proved.** `em487d`. Applying and reverting an Audition digest-exact left an
empty named transaction in front of the producer's history and hid a pending
redo.

**Duet.** Each Audition state application begins a fresh transaction around the
null writes and synchronously dispatches the engine's answers, then calls
`undoCurrentTransactionOnly`. JUCE removes only those answers and restores its
stashed redo transactions; the direct null writes remain.

### Appended detached items do not advance an Edit's item-ID allocator

**The engine.** Adding a detached ValueTree subtree carrying item IDs does not
pass through `Edit::createNewItemID`, so the Edit's in-memory allocator does not
advance. A graph may also retain an item after its transient tree has been
removed. The next ordinary creation can therefore reuse the retained item's ID.

**Where.** `Edit::createNewItemID` and the Edit item registries.

**Proved.** `em487d`. Accepting immediately after Audition registered the new
track and its plugins over the retained transient items with the same IDs;
repeated A/B did the same when each detached Edit began its allocator again.

**Duet.** The detached Edit is reused across A/B cycles, and before detached
state enters the real Edit its item-ID allocator is advanced beyond every ID in
that state. Accepting then allocates IDs beyond the transient ones.

### The render's solo isolator also unmutes the tracks it is given

**The engine.** `FreezePointPlugin::ScopedTrackSoloIsolator` solo-isolates every
track in the array it is handed and clears the mute on each one that is muted,
restoring both in its destructor. A render under it therefore holds each of
those tracks whatever the project says about them, and `Renderer::renderToFile`
puts one around every render.

**Where.** `ScopedTrackSoloIsolator` in `tracktion_FreezePoint.cpp`; the
construction site is `Renderer::renderToFile` in `tracktion_Renderer.cpp`.

**Proved.** `sh2dkg`, and `6zog6s` before it: a whole-project render of a
two-track edit with the 440 Hz track muted held that tone at −9.02 dB, the same
level as the render with nothing muted.

**Duet.** `Session::renderToFile` renders without the isolator, so mute and solo
reach the file — a render of the whole project is what the producer hears.
`Session::renderTrackToFile` keeps it, because a render of one track is a
measurement of that track and has to ignore what is soloed elsewhere.

### A PropertyStorage is a whole `Settings.xml`, held from the moment it is read

**The engine.** `PropertyStorage` reads `Settings.xml` under the app prefs
folder on first use and keeps that set in a `juce::PropertiesFile`, which writes
the whole set it holds every time it saves — on `save()`, on its two-second
timer, and from its destructor. Two of them open on one path therefore write
over each other: whichever saves last decides the file, and the keys the other
added since it read are gone. `te::Engine`'s constructor takes ownership of a
`unique_ptr<PropertyStorage>`, so one instance cannot be handed to two Engines;
every default method of the class routes through the virtual
`getPropertiesFile()`.

**Where.** `PropertyStorage::getPropertiesFile` in
`tracktion_PropertyStorage.cpp`, and the `Engine` constructors in
`tracktion_Engine.cpp`.

**Proved.** `uztxbx`: with the shell's store and a session's engine each holding
one of their own, a value written through the shell while the project was open
was gone from the file once the session closed — the engine's snapshot, taken
when its Engine was made, went out over it.

**Duet.** One store per process, `DuetPropertyStorage`, reached through a
`juce::SharedResourcePointer`: `duet::model::AppSettings` is the engine-free
handle the shell holds one through, and each session's Engine is given a
`SharedPropertyStorage` — a forwarding adapter that owns a reference to the same
store rather than a second one.

### The engine's ArrangerTrack is a track, and would be counted as one

**The engine.** `ArrangerTrack` holds `ArrangerClip`s — named sections of an
Edit, moved around as blocks — and it is a `ClipTrack`, so `getAllTracks`
returns it, a render's track bit set indexes past it, and every graph the engine
builds walks it. Nothing on `Edit` makes one: there is no `getArrangerTrack`, and
the type appears in the engine's own sources only in the umbrella header, the
module build file, and the two `isArrangerTrack` predicates.

**Where.** `model/tracks/tracktion_ArrangerTrack.h` and
`model/clips/tracktion_ArrangerClip.h`; `te::getAllTracks` in
`tracktion_TrackUtils.h`.

**Proved.** `v5yhh1`, reading the engine sources while deciding where the
arrangement's sections live.

**Duet.** Sections are Duet's own state: a `DUETSECTIONS` child on `edit.state`
holding a name and a bar range each, written through the Edit's UndoManager and
carried by a save because a save copies that tree whole (ADR 0005). The
alternative was a whole track, joining every track list and every render bit set,
to carry three strings.

### A plugin says what its value means, and not always about that value

**The engine.** Every `AutomatableParameter` has a `valueToString`, and `getLabel`
is empty on all of the engine's own plugins, so the only place a built-in states
a unit is inside that display string. The string is not always about the value
beside it: `CompressorPlugin` holds its ratio as 0.05 and displays it as
`20.00 : 1`, and holds its threshold as a gain of 0.501 and displays it as
`-6.02 dB`, while `EqualiserPlugin` displays a frequency of 80 as `80 Hz`.

**Where.** `AutomatableParameter::getLabel` and `valueToString` in
`model/automation/tracktion_AutomatableParameter.h`; the parameter definitions at
the top of `plugins/effects/tracktion_Compressor.cpp` and the
`EQAutomatableParameter` in `plugins/effects/tracktion_Equaliser.cpp`.

**Proved.** `v5yhh1`, giving `PluginParameterInfo` a unit for the Tool
Vocabulary.

**Duet.** The facade converts, so that a built-in's value is the producer's
number in both directions: the compressor's ratio is 4 for four to one and its
threshold is in decibels, and `setPluginParameter` takes back exactly what
`pluginParameters` gave. The conversion is one table beside `engineTypeOf` in
`EditOps.cpp`, one entry per parameter of every built-in Duet ships, and it also
states the unit — the display string is not read for it, because a plugin's own
text is about the engine's number and a parameter's units must not depend on
what its value happens to be. A parameter with no producer's unit — a filter's Q,
a reverb's freeze, an oscillator's pan — says so with an empty unit. Proved
again at `v6ac5c`.

### The engine's plugin scanner and a hosted VST3 both race under TSan

**The engine.** `linux-tsan` reports 34 ThreadSanitizer warnings from the single
case that scans a VST3 and inserts it, and none of the memory is Duet's. They are
of two kinds: data races on the out-of-process scanner's array of pending
replies, which the main thread reads while the scan thread appends to it with no
lock between them; and `destroy of a locked mutex` from
`juce::MessageManagerLock`'s destructor, reached through a hosted VST3's
parameter read and again from the fixture plugin's own copy of JUCE at teardown.

**Where.** `PluginScanHelpers::PluginScanMasterProcess::findReply` and its
`OwnedArray<juce::XmlElement>`; `juce::MessageManagerLock`'s destructor reached
from `VST3PluginInstanceHeadless::VST3Parameter::getText`.

**Proved.** `wyfdjb`, on the dev machine 2026-08-26 under
`setarch $(uname -m) -R` with `TSAN_OPTIONS=detect_deadlocks=0`.

**Duet.** Nothing. ADR 0006 trusts Tracktion Engine outright and puts only Duet's
own code under the sanitizers, and neither report is reached from a Duet thread.
A TSan report from this case is expected noise; one that names Duet memory is
not.

### A parameter remembers what it was handed, not what it uses

**The engine.** `AutomatableParameter::setParameter` stores the value it is given
in `currentParameterValue` untouched, and only the value it processes with —
`currentValue`, and `currentBaseValue` — goes through `getValueRange().clipValue`.
`getCurrentExplicitValue()` returns the first of those. A parameter set outside
its range therefore reads back as it was set while the plugin uses another
number, and the display string, which is also built from the explicit value,
agrees with the read rather than with the sound: a compressor ratio set to 4 on
the engine's own 0..0.95 scale reads back 4, displays `0.25 : 1`, and compresses
at 1.05 to one.

**Where.** `AutomatableParameter::setParameter` and `setParameterValue` in
`model/automation/tracktion_AutomatableParameter.cpp`;
`getCurrentExplicitValue` in the matching header.

**Proved.** `v6ac5c`, on the dev machine 2026-08-26, by setting a compressor's
ratio to 4 through the facade and reading the display string back.

**Duet.** `EditOps::setPluginParameter` holds the value inside the parameter's
range itself, after converting it, so that the number the facade reports back is
the number the plugin uses. The engine's own clip is left in place underneath;
this only stops the facade from reporting a value that was never adopted.

### The transport ends a take whose playhead is not rolling, on the message loop

**The engine.** `TransportControl` runs a 50 Hz timer on the message thread, and
every tick a transport that says it is recording while the playback graph's
playhead is not playing is stopped:
`if (! playHeadWrapper->isPlaying()) { if (isRecording()) { stop (false, false); return; } }`.
Freeing or reloading the playback context therefore does not end a take by
itself — the next turn of the message loop does. That is the whole answer to what
can stop a take that has begun: anything that leaves the graph without a rolling
playhead, plus a message loop.

**Where.** `TransportControl::timerCallback`;
`TransportControl::PlayHeadWrapper::isPlaying`, which reads the playhead out of
the current `EditPlaybackContext`.

**Proved.** `3ho6tg`. `an undo during a take neither stops it nor moves the
playhead` was made to fail on demand, at the assertion it failed at in the wild,
by calling `Session::rebuildDevices` on the rolling take.

**Duet.** Nothing asks a stopped take to record again — hazard 6's recording
remedy is a pre-roll and not a retry, because a second ask would land the first
half as one clip and begin another. So a headless take test has any device
rebuild happen before the take rather than during it, and spends as little
message loop as it can while a take is rolling. `a commanded device rebuild ends
a take, and the model starts no other` pins that contract.
