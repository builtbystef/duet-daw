---
id: qf9e9h
title: One raising plugin takes down every read of the track it is on
state: done
assignee: claude
priority: low
labels:
    - bug
parent: js437t
created: 2026-08-30T02:57:53Z
updated: 2026-08-30T19:23:04Z
---

## What is wrong

97ynt7 made a parameter read that raises on an already-hosted plugin an error
result the run survives: `ProjectTools::read` catches what the hosting layer
throws and answers the model with an error instead of carrying the exception
into the message loop. That is the right floor, and it is coarse.

The catch is around the whole read, so the failure is the whole tool call. A
track carrying one hostile plugin answers nothing at all to `get_plugin_chain`,
and — because `automatedTargetsOf` reads every plugin's parameters to find the
drawn curves — nothing at all to `list_tracks` either, for every track in the
project. The model can still ask about the arrangement and the MIDI, but the two
tools that would tell it what is on the tracks are gone until the producer takes
the plugin out.

Measured on the dev machine 2026-08-29 with the raising VST3 fixture
(`tests/vst3_fixtures/RaisingPlugin.cpp`): `get_plugin_chain` and `list_tracks`
both answered `internalError`, and `get_arrangement` answered normally.

## What to decide, then build

Whether a plugin that raises is confined to its own entry — the chain answers
with every other plugin and marks that one as unreadable, and the track list
leaves its curves out rather than failing — and what the seam calls that state.
`available: false` already names a plugin the machine does not have; a plugin
that is there and will not answer may or may not be the same thing to a model.

## Acceptance criteria

- [ ] A chain holding one raising plugin answers with every other plugin in it,
      in order, and says of the raising one that it could not be read.
- [ ] `list_tracks` answers for a project holding a raising plugin.
- [ ] The run still survives, and nothing the hosting layer throws reaches the
      message loop.

## Notes

**claude** — 2026-08-30T19:23:04Z

Built 2026-08-30. All three criteria met and asserted at the protocol seam
(seam 1 of the spec's Testing Decisions), with the raising VST3 fixture on a
real project and the test-double sidecar making the calls.

## THE DECISION

A plugin that raises is confined to its own entry, and the seam calls that state
`parametersReadable`. It sits beside `available` on every chain entry and the
two name different things: `available: false` is a plugin the project names and
this machine does not have; `parametersReadable: false` is a plugin that is here
and loaded and would not say what it is set to. What it is and where it sits are
still known, so the entry is complete but for its `parameters`, which is `[]`
rather than a short list — the model's read is discarded whole rather than kept
partial, because a chain entry listing three of a plugin's parameters and
saying nothing about that would be worse than one listing none. Both booleans
are always present, so the result shape does not move.

`list_tracks` keeps the spec's shape and gains no field: a plugin that will not
enumerate its parameters simply contributes no curves to `automatedParameters`,
and the track's own fader and pan curves, its plugin names, its mixer and every
other track answer as they always did. `get_automation` follows the same rule,
since it reads the same list of drawn curves.

## WHAT LANDED

- `parametersOf` in `ProjectTools.cpp` is now the one place the vocabulary reads
  a plugin's parameters — the three call sites (the chain, the drawn-curve list,
  the curve names) all go through it — and it catches. It catches `...` and not
  `std::exception`: what a plugin throws is the plugin's to choose, nothing here
  reads the message, and a plugin throwing a type of its own would otherwise be
  the raise that reached the message loop after all. `ProjectTools::read` gained
  the same total catch and stays as the floor under everything else in a read.
- `get_plugin_chain` entries carry `parametersReadable`. The sidecar's
  description of the tool tells the model what it means, beside what it already
  says about `available`.
- Two tests in `ProjectToolsTests.cpp`. The first replaces the coarse assertion
  97ynt7 left (`get_plugin_chain` answering `internalError`), which asserted the
  behavior this issue exists to change: a chain of [raising fixture, built-in
  EQ] now answers with both, in order, the fixture with `parametersReadable`
  false and no parameters and the EQ with its own, and `get_arrangement` after
  it still answers. The second holds a raising plugin on one track of a
  two-track project and asserts `list_tracks` answers for both — the other
  track's plugins and curves untouched, the hostile track's pan curve still
  listed and only the plugin's own curves gone — and that `get_automation` on
  that track answers the same way.

## FACTS FOR A REVIEWER

- The engine builds a plugin's parameter list in one pass, so a raise partway
  through loses the whole list, including the engine's own Dry and Wet levels.
  That is why `parameters` is empty and not partial; nothing was chosen there.
- The confinement was proved to be what the second test asserts: with
  `automatedTargetsOf` put back to reading the model directly, the track-list
  test fails with no `tracks` key at all, which is exactly the coarse failure
  this issue describes.
- ENGINE_NOTES.md's entry on a raising `getText` and ARCHITECTURE.md's paragraph
  on the tool layer both now say what a raise costs.
- Every check was run over a full build: format clean, the lint sweep clean, and
  608 of 608 tests pass (the one skip is the build-without-the-ML-runtime test,
  as always in this configuration).
- Published zxpgna: the same raise is unguarded everywhere else Duet asks a
  plugin what its values mean — `SuggestTool`'s validation of `plugin.setParam`
  (whose marshalled read has no catch at all), the Suggestion manager's
  staleness description, and two places in the interface. Read from the sources,
  not measured; none of it is this issue's contract, and the read tools are what
  this issue named.
