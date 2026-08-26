---
id: v6ac5c
title: A built-in plugin parameter's value is the engine's number, not the producer's
state: todo
priority: medium
parent: b1j3me
created: 2026-08-26T15:11:00Z
updated: 2026-08-26T15:11:00Z
---

## What is wrong

`duet::model::PluginParameterInfo` says a built-in's value is "in the real units
the producer sees", and for the engine's compressor it is not. The engine holds
that plugin's ratio as `1 / ratio` in 0..0.95 and displays it as `20.00 : 1`, and
holds its threshold as a gain in 0.01..1 and displays it as `-6.02 dB`. The EQ is
honest — a frequency of 80 is 80 Hz — so the facade's claim is true of some
built-ins and false of others, with nothing in the read that says which.

This reaches the Collaborator twice. `get_plugin_chain` hands it those numbers as
bare scalars, which is right — they are what the project holds — but a model
that reads "ratio 0.05" and wants four to one has no way to know what to write;
and `plugin.setParam` (cwz0of) takes "builtin: real units" from spec js437t, so a
Suggestion that sets ratio 4.0 gets 0.95 after clamping, which is about one to
one, silently.

Found while building the project-read tools (v5yhh1). The tools do not make it
worse: `PluginParameterInfo::unit` is derived from the display string only when
the display's own number is the value, so a compressor's ratio and threshold come
back with no unit rather than a misleading one. That keeps the read honest and
leaves the write unsolved.

## What to decide, then build

Either the facade converts — a built-in's value crosses in the units its name
implies, both ways, and the conversion is one table beside `engineTypeOf` — or
the facade stops claiming it does and the vocabulary carries the range and the
display string so that a caller can see what a number means. The first is what
js437t's `plugin.setParam` already assumes.

## Acceptance criteria

- [ ] Every built-in parameter Duet ships either reads back in the units its name
      implies or says plainly that it does not, and the header's claim matches.
- [ ] Worked: setting the built-in compressor's ratio to 4.0 and reading it back
      gives 4.0, or the vocabulary makes clear what number to write instead.
- [ ] `get_plugin_chain` carries whatever the decision leaves the Collaborator
      needing in order to write a parameter it has just read.
