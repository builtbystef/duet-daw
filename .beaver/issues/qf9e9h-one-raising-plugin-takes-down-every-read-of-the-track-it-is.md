---
id: qf9e9h
title: One raising plugin takes down every read of the track it is on
state: todo
priority: low
labels:
    - bug
parent: js437t
created: 2026-08-30T02:57:53Z
updated: 2026-08-30T02:57:53Z
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
