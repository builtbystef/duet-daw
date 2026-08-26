---
id: v5yhh1
title: 'Project-read tools: the Tool Vocabulary against the live project'
state: done
assignee: claude
priority: medium
depends_on:
    - xy9438
    - 4r7nlj
parent: js437t
created: 2026-08-12T04:01:44Z
updated: 2026-08-26T15:19:33Z
---

## What to build

The five project-read tools of the Tool Vocabulary, dispatched from the sidecar's tool calls and answered from the live project model: the track list with its mixer state and routing, the arrangement with its sections and clip placement, MIDI note lists, automation lanes, and the plugin chain for engine built-in plugins. Provenance is structural: everything read from the project model crosses the seam as a bare scalar, because a bare value is by construction a fact. Buses are tracks — the master and every group are read through these same tools.

Reads execute on the message thread, the sole writer of the project model; the service thread marshals and waits. They read the authoritative state, never a second copy. The fod077 fixtures, rebuilt as real projects with their recorded defects fixed, become the regression corpus, driven through the protocol seam against the test-double sidecar.

## Acceptance criteria

- [ ] Each of the five tools answers a tool call with the fields the spec's contract names, and an unknown tool name yields an error result the run survives.
- [ ] Track list, worked: a MIDI track named "Bass" at −6.0 dB, pan 0, unmuted and unsoloed, one send to a bus at −12.0 dB, two clips, one built-in EQ, and volume automation → one entry with exactly those values, its output naming the bus track's id, and its automated parameters naming volume.
- [ ] The master bus and every group appear in the track list with their own kind, and each is accepted as a track id by every other tool that takes one.
- [ ] Arrangement, worked: a project at 128 BPM in 4/4 with an 8-bar "Intro" section and a clip starting at bar 5 for 4 bars, looped → exactly those numbers; a project that declares a key reports it as a bare value, and one that declares none omits the field entirely.
- [ ] MIDI, worked: a clip holding a note of pitch 60, start 1.0 beats, length 0.5 beats, velocity 100 returns exactly that note with a stable id; asking for a track without naming a clip returns every MIDI clip on that track.
- [ ] Automation, worked: a volume lane with points at 0.0 and 4.0 beats returns both, with their values, in time order; a plugin-parameter lane names its plugin and parameter.
- [ ] Plugin chain, worked: a track carrying a built-in EQ returns that plugin in chain order with its enabled state, its latency, and its parameters as bare scalars with names and units.
- [ ] A tool call naming a track, clip, or plugin id that does not exist returns an error the model can correct against — never a crash, never an empty success.
- [ ] Every tool result is produced by reading the authoritative project model on the message thread; no tool code runs on the audio thread and none of it takes a lock the audio callback can take.
- [ ] Prompt-cache discipline: the same project state serializes byte-identically twice, with stable content ahead of volatile content and no timestamps anywhere in a result.
- [ ] The fixture corpus is served end to end through the protocol seam, and every fixture's expected values are asserted from it.

## Notes

**claude** — 2026-08-17T04:12:26Z

Scope note (2026-08-17): rebuilding the fod077 fixtures as real project files — their two recorded defects fixed (fixture-a pitch naming, fixture-f pad/Rhodes rub) and their shapes updated to the settled schemas (Estimate wrapper, get_automation, display-string plugin params) — is in-scope of this slice, as its criteria already assume.

**claude** — 2026-08-26T15:19:22Z

Completed 2026-08-26. The five project-read tools of the Tool Vocabulary answer
tool calls from the live project model, over the real socket against the
test-double sidecar, and the fod077 corpus is rebuilt as seven real projects that
every one of them is asserted against.

## WHAT LANDED

- `modules/duet_collab/include/duet/collab/ToolDispatch.h` + `src/ToolDispatch.cpp`
  — `ToolCall` and `ToolRegistry`: the closed set the vocabulary is, and the
  dispatch to it. `JsonRpc.h` gains `rpcError::unknownTool` (-32002).
- `modules/duet_collab/include/duet/collab/ProjectTools.h` + `src/ProjectTools.cpp`,
  in a new target `duet::collab_tools` — `list_tracks`, `get_arrangement`,
  `get_midi`, `get_automation`, `get_plugin_chain`, plus `toolId`, the id shape the
  seam carries, and `ProjectReadMarshal`, the function that puts a read on the
  message thread.
- `duet_model` grew what those reads needed: `SectionInfo` with
  `Session::sections()` and `EditOps::setSections`, `Session::key()` and
  `EditOps::setKey`, `Session::barAtSeconds` and `beatsAtSeconds`,
  `PluginInfo::latencySeconds`, and `PluginParameterInfo::unit`.
- `tests/fixtures/collaborator/` — the corpus: seven fixtures and a README that
  says what a fixture holds and what the rebuild changed.
- `tests/ProjectToolsHarness.h`, `tests/ProjectToolsTests.cpp` (11 cases),
  `tests/ToolFixtureTests.cpp` (7 runs of one case, 4,912 assertions);
  `tests/sidecar_double` gains the `call-tools` script; test support gains
  `messageThreadMarshal`.

## DECISIONS A REVIEWER NEEDS

1. **Two targets, one module.** `duet::collab` must link no engine — that is what
   makes the real-time rule structural — and reading the project means linking the
   model, which links the engine. So `duet::collab_tools` is a second target in the
   same module, the split `duet_gui` already makes. What crosses between them is
   JSON and one `std::function` that runs a read on the message thread and waits.
   The service never learns what a tool is; the tools never learn what a socket is.

2. **The marshal is injected, and it is the whole of the real-time rule for a
   tool.** `duet_collab_tools` links no JUCE either, so it cannot reach a message
   thread on its own; whoever wires the Collaborator into the DAW supplies the
   marshal. The suite supplies `duet::testing::messageThreadMarshal`, and the
   thread test wraps it to record the thread each read ran on: five reads, all on
   the message thread, none on the service's.

3. **`Json` is now `nlohmann::ordered_json`.** An object keeps the order it was
   written in, which is what makes "stable content ahead of volatile content"
   expressible: a track states its id, name, kind, routing, clips, plugins and
   curves, and its mixer last, so a fader move invalidates the tail of a cached
   result rather than its middle. Two objects with the same members in a different
   order are no longer equal — compare members, not whole objects. The sidecar
   double is ordered too, or it would sort the keys of everything it relays and the
   discipline could not be asserted through it.

4. **Ids are `track-12`, `clip-13`, `plugin-14`, `note-1`, and `track-master`.**
   A note's handle is Duet's own and counts from one, so it would collide with a
   track's without the kind in front; and an error about `track-99` says what was
   being looked for. The master's ref is `UINT64_MAX`, which says nothing to anyone
   reading a trace, so it is `track-master`.

5. **Buses are tracks, and the master is the one with no output.** `list_tracks`
   ends with the master, every other track names where it goes — the bus it feeds,
   or the master when it feeds nothing in particular — and the absence of `output`
   is what says a track is the end of the signal. Every tool that takes a track id
   takes the master's and every group's.

6. **Sections and the key are Duet's own state on the Edit's own tree.** Neither
   existed. The engine has an `ArrangerTrack` that holds named sections, and it is
   a *track*: it would join every track list, every render bit set and every graph
   the engine builds, to carry three strings. So `DUETSECTIONS` and
   `duetProjectKey` sit on `edit.state` beside `duetMasterMuted`, written through
   the UndoManager and carried by a save because a save copies that tree whole
   (ADR 0005). `EditOps::setSections` states the whole list at once, like an
   automation curve's points; `setKey` with an empty key is a project that declares
   none, and no property at all is what "declares none" is.

7. **A declared key crosses bare.** js437t's contract table types
   `get_arrangement.key` as `Estimate<string>`, and this issue's criterion, the
   spec's own prose and the provenance rule all say a declared key is a fact read
   from the project model. Built to the rule. Published as 9alksy, with the
   contract's other loose end (`role`, which nothing in milestone one declares, and
   which the tool therefore omits).

8. **A unit is only a unit when the plugin's text is about that value.** The
   engine's plugins have no label; the only place a built-in states a unit is
   inside its display string. But that string is not always about the value beside
   it: the compressor holds a ratio of 0.05 and displays `20.00 : 1`, and a
   threshold of 0.501 and displays `-6.02 dB`. So `PluginParameterInfo::unit` is
   the text after the number, and only when that number is the value — a
   frequency of 80 displayed `80 Hz` has the unit `Hz`, and the ratio has none
   rather than `: 1`. The deeper problem, that a built-in's value is the engine's
   number and not the producer's, is published as v6ac5c: it is the write path's,
   and it would silently turn a Suggestion's 4:1 into 1:1.

9. **Latency crosses in samples at a rate the model states, not the device's.**
   The project has no sample rate — a rate belongs to the device or the render it
   is played through — and a tool result that changed when an audio device opened
   would be a cache buster for a fact that did not move. Every milestone-one
   built-in reports no latency at all, whatever the rate is.

10. **A track's curves are the ones with points on them.** A parameter every
    plugin owns and nobody has drawn is not automation, so `automatedParameters`
    and `get_automation` both ask the model for each candidate curve and keep the
    ones that answer. A lane is named "volume", "pan", or "plugin: parameter".

11. **`get_midi` sorts.** In time, and within one moment from the lowest pitch up,
    so that a chord reads the way it is voiced and the same clip serialises the
    same way however its notes were written. Two fixtures failed before this and
    were right to.

12. **A scanned plugin's display text is the one wrapped value.** `format` is
    `vst3`, parameters carry `vendorName` and `normalizedValue` bare, and the text
    that says what that number means crosses as an `Estimate` — Duet owns what its
    own devices mean and not what a stranger's do. The estimate *ledger*, which
    marks a whole run, belongs to a later slice; this is the wrapper the ledger
    will watch for.

## THE FIXTURE CORPUS

`tests/fixtures/collaborator/README.md` is the reference. Three things a reviewer
should know:

- **A fixture is both the recipe and the expectation.** `ToolFixtureTests` builds
  the project from the file through the edit vocabulary, then asserts every value
  the file states against what came back over the socket. Nothing is asserted that
  the file does not say.
- **The rebuild dropped the analysis and the plugins Duet does not have.** fod077's
  RMS, LUFS and spectral bands were written by hand; a real project's are measured,
  and they come back with 3bgymu. A chain naming a saturator, a filter, a chorus or
  a bit crusher has no Duet device to be, so each file's `rebuild.dropped` says
  which. Sends now land in real group tracks, listed in `rebuild.bussesAdded`.
- **Both recorded defects are fixed.** fixture-a named pitch 36 `C1` on the kick
  and `C2` on the bass; the rebuild has no note names to disagree about, because a
  tool result carries the pitch and nothing else. fixture-f's pad held Dm and F
  across the Rhodes' Bb and Am bars — its pad follows the changes now (Dm, F, Bb,
  Am, a bar each), so the control fixture is unambiguous. Its filter sweep in the
  break is an EQ frequency sweep, which is the same gesture on the devices Duet
  ships.

## TESTING

Everything is asserted through the spec's primary seam: a real service, a real
Unix socket, the test-double sidecar as a real child process, and a real project.
Two mechanics worth knowing before the next slice:

- **A tool run cannot be waited on by blocking.** Every read marshals to the
  message thread, so a test that blocked would be the thread the answer is waiting
  for. `ToolRun` waits with `pumpUntil`, and for the same reason a run is started
  with `startRun` rather than `configure`: `configure` blocks its caller, and the
  double's first tool call arrives before its answer to `configure` does.
- **`ToolRun::result` returns a reference.** It first returned by value, and every
  `const auto& x = run.result (0).at ("y")` in the suite was a dangling reference
  into a dead temporary — three tests failed with correct data on the wire. The
  responses are gathered once in the constructor now, so the class of bug is gone
  rather than fixed.

## CHECKS — all four green

- Format: `clang-format-18 --dry-run --Werror` over `git ls-files` — clean.
- Lint: `./scripts/lint.sh`, full sweep — clean. Findings on the way there were
  real: a reference data member on a copyable class, a const copy of a result, a
  `const auto` that wanted `const auto* const`.
- Build: `cmake --build --preset linux-debug -j 4` — clean.
- Test: `ctest --preset linux-debug` — 330/330, of which 18 are this slice's
  (11 worked examples and one fixture case that runs seven times).

Beyond the four, because a marshal across two threads is the substance here:

- `linux-asan` (ASan + UBSan, vptr on): every `[collab]` case, no sanitizer output.
- `linux-tsan` (TSan + UBSan), under `setarch -R`: every `[collab]` case but one
  passes with **zero** ThreadSanitizer warnings — the service thread and the
  marshal that carries a read to the message thread are clean. The exception is
  the scanned-plugin case, which reports 21, all of them in the engine's
  out-of-process scanner or in a `MessageManagerLock` destroyed inside a hosted
  VST3. They are not this slice's: the existing PluginHostingTests case that
  scans and hosts the same fixture reports 34 of the same kind on its own.
  Recorded as wyfdjb. Two mechanics for whoever runs it next: the deadlock
  detector's own CHECK fires inside TSan on this suite, so run it with
  `TSAN_OPTIONS=detect_deadlocks=0`, which leaves race detection on; and the
  build still exits non-zero at Catch2's test-discovery step for the documented
  memory-mapping reason, with a working binary beside it.

## FACTS FOR THE NEXT SESSION

- The service is still not wired into `duet_app`, and nothing here asks for it.
  Wiring means handing `ProjectTools` a `Session` and a marshal built on
  `juce::MessageManager`, and registering `ToolRegistry::call` as the `tool.call`
  handler — three lines, and they are the shell's.
- `ToolRegistry` is the seam the write tool joins: `suggest` (cwz0of) is another
  `add`, and `toolId::toTrack`/`toClip`/`toPlugin` are how it will read the ids a
  model hands back.
- The estimate ledger has one thing to watch for so far: the `displayString`
  wrapper `get_plugin_chain` puts on a scanned plugin's text.
- `get_track_analysis` (3bgymu) and `estimate_audio_content` join the same
  registry, and the corpus is waiting for them: the fixtures are real projects
  now, so their analysis is measurable rather than declared.
