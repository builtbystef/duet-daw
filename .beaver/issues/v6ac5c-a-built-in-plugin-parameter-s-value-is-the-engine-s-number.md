---
id: v6ac5c
title: A built-in plugin parameter's value is the engine's number, not the producer's
state: todo
priority: medium
parent: b1j3me
created: 2026-08-26T15:11:00Z
updated: 2026-08-26T15:38:27Z
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

## Notes

**claude** — 2026-08-26T15:38:27Z

Decision (2026-08-26, user): **a built-in plugin parameter reads and writes in real units, and Duet does the conversion at the facade.** This settles the open half of every criterion here — the 'or' branches are not taken.

**What it means.** `get_plugin_chain` reports the built-in compressor's ratio as 4.0, not 0.05, and its threshold in dB, not as a gain. `plugin.setParam` (cwz0of) takes the same number back. Reading a parameter and writing what was read is the identity, which is what criterion two asks for. `PluginParameterInfo`'s existing claim — values are in the real units the producer sees — becomes true rather than being weakened.

**Why, and not the alternative.** Exposing the engine's own number with a display string beside it would leave the model to derive the reciprocal mapping from '20.00 : 1'. Spec js437t's premise is that the Collaborator never receives a guess disguised as a fact and reasons from what native code computed; asking it to infer a mapping Duet already knows inverts that. The failure is also silent: a model writing ratio 4.0 against the raw scale is clamped to 0.95, about one to one, and nothing says so. A producer then hears a compressor that is not compressing, with no way to trace it.

**Provenance is not weakened.** A unit conversion of project state is still a fact, computed by native code, in the same category as a measured loudness. It crosses bare, not wrapped in an Estimate.

**Two things this decision binds, which the criteria do not say outright:**

1. **Automation lanes translate on the same scale.** `get_automation` returns plugin-parameter lane values, and a lane that reported 0.05 while `get_plugin_chain` reported 4.0 would replace one inconsistency with a worse one. One scale per parameter, wherever it is read.

2. **External plugins are out of scope and stay raw.** Duet cannot know a third party's mapping, and 97ynt7 already makes external parameters estimates. Two regimes is the correct outcome, and it lines up with the fact/Estimate split the spec already draws: a built-in is Duet's own and it knows the mapping; an external one is opaque and is hedged.

**The escape hatch stays open.** Where a built-in parameter has no sensible real-unit form, say so through the unit field rather than inventing one — criterion one already allows this ('or says plainly that it does not'). It is the exception, not the default.

Build against this; no further escalation is needed on the question of which number crosses the seam.
