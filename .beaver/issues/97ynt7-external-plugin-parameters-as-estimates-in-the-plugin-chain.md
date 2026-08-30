---
id: 97ynt7
title: External plugin parameters as estimates in the plugin chain
state: done
assignee: claude
priority: medium
depends_on:
    - 2z0y5u
    - aty85a
parent: js437t
created: 2026-08-12T04:02:25Z
updated: 2026-08-30T03:15:38Z
---

## What to build

A hosted VST3's parameters cross the seam differently from a built-in's. Duet owns its built-ins' semantics, so their parameters are bare scalars in real units. An external plugin's meaning belongs to its vendor, so its parameters carry the vendor's own name, a normalized value in 0..1, and the plugin's own UI display text wrapped as an estimate — the display string is what the plugin says, and what it means is a guess. That wrapped string feeds the same estimate ledger, so a run that inspected an external plugin's parameters marks its output exactly as a key estimate would.

Each plugin in the chain also reports its format, so the model can tell the two kinds apart without inferring it.

## Acceptance criteria

- [ ] Worked: a track carrying a built-in compressor and a hosted VST3 → the built-in's parameters are bare scalars in real units with names and units; the VST3's parameters carry vendor name, a normalized value in 0..1, and a wrapped display string; each plugin reports its format and its latency.
- [ ] The wrapped display string is the plugin's own text, unaltered, and its method says exactly that.
- [ ] Reading an external plugin's parameters taints the run's estimate ledger; reading only built-ins leaves the ledger empty.
- [ ] Plugins appear in chain order, and a disabled plugin reports itself disabled while still listing its parameters.
- [ ] A plugin that is missing or fails to load appears in the chain with its name and is reported as unavailable, never omitted silently.
- [ ] No plugin scan and no plugin load happens as a side effect of a tool call: only plugins already in the project's chains are reported.
- [ ] A parameter read that fails on an already-hosted plugin (an error or exception surfaced through the hosting layer) returns an error result the run survives: the run continues, and the DAW keeps working. Crash isolation is explicitly not asserted — hosting is in-process (b1j3me, hvv3nn; only scanning is out of process), so a plugin that brings the process down brings the DAW down; surviving that arrives only with milestone-two out-of-process hosting.

## Notes

**claude** — 2026-08-28T21:17:48Z

2z0y5u built the wrapper and the ledger, and left this issue one half of its own
work that it did not have before.

`Estimate` and `wrapped()` in `duet/collab/Estimate.h` are now the one shape a
guess crosses the seam in, and `ProjectTools`'s `displayString` uses them, so
what a scanned plugin's display text looks like on the wire is already right.
What it does not do is write itself into the run's `EstimateLedger`, and the
spec names that value beside `estimate_audio_content` as something the ledger
holds. So a run whose only guess was a plugin's display text is not marked as
based on estimates, and it should be.

The mechanism is there and takes a constructor argument: `EstimateLedger::record`
is what answers with the wrapped value — wrapping and recording are one act — and
`ContentEstimates` shows the shape. `ProjectTools` needs the ledger passed in
(`ToolRun` already carries one, `ToolRunOptions::ledger`), and a test that reads
a chain holding one of the VST3 fixtures and asserts the run is marked.

**claude** — 2026-08-30T03:15:38Z

Built 2026-08-29. Every acceptance criterion is met and asserted at the protocol
seam the spec names: a real service, a real project, and the test-double sidecar
making the calls (`tests/ProjectToolsHarness.h`).

## WHAT LANDED

- `ProjectTools` takes an `EstimateLedger*`. A scanned plugin's display text is
  now wrapped and recorded in one act — `EstimateLedger::record` is what answers
  with the wrapper — so a run that read a hosted plugin's parameters is marked
  as based on estimates exactly as a run handed a guessed key is. The ledger
  line names the plugin and the parameter: `plugin-14.gain.displayString`. Given
  no ledger the text still crosses wrapped and nothing is marked.
- `duet::collab::displayStringMethod` is the method those wrappers carry, and it
  says what the criterion asks it to say: "the plugin's own display text,
  unaltered". A test asserts the wrapped value is byte-for-byte what
  `Session::pluginParameters` read off the plugin.
- Every chain entry carries `available`. A plugin the project names and the
  machine does not have is in the chain, in order, with its name and
  `available: false` — never omitted. The sidecar's `get_plugin_chain`
  description tells the model what that means.
- `ProjectTools::read` catches inside the marshalled read. The read runs on the
  message thread, so an exception out of the hosting layer would have had the
  DAW's own loop under it and nothing to catch it; it now becomes the model's
  error result and the run goes on to its next call.
- `tests/vst3_fixtures/RaisingPlugin.cpp` — a third VST3 fixture, well behaved
  until a marker file is dropped beside its bundle, then raising from its
  parameter's `getText`. That is what makes the criterion's scenario real: a
  read failing on a plugin the producer already has in a chain, not one that
  could never be loaded.

## FACTS FOR A REVIEWER

- The exception does cross the plugin boundary. `std::runtime_error` thrown
  inside the fixture arrived in the host with its message intact, through JUCE's
  VST3 wrapper on both sides — the test asserts the model was told the fixture's
  own words. Recorded in ENGINE_NOTES.md.
- Tracktion adds two parameters of its own — "Dry Level" and "Wet Level" — ahead
  of the vendor's on every hosted plugin, and they are indistinguishable in
  shape from the vendor's. So they cross in the external shape and write ledger
  lines, which is wrong twice over: the name is the engine's and the display
  text is the engine's. Published as 1mldqz; the worked example here looks the
  vendor's parameter up by name rather than taking the first. Recorded in
  ENGINE_NOTES.md.
- The catch is around the whole read, so one raising plugin costs the whole
  tool call — and `list_tracks` too, because `automatedTargetsOf` reads every
  plugin's parameters. The criterion's floor is met (an error result, the run
  survives, the DAW keeps working) and confining the failure to the plugin that
  raised is a contract decision beyond it. Published as qf9e9h.
- Chain order and a disabled plugin still listing its parameters were already
  asserted by the built-in chain test from v5yhh1; nothing here changed them.
- Every check was run over a full build: format clean, 558 tests pass, and the
  lint sweep reports exactly one error, in `PluginScanDialog.cpp`, which landed
  with e02ea08 and is untouched by this diff. Published as ssjy4l; `main` is red
  on lint until it is fixed. This issue's own files lint clean.
