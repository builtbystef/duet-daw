---
id: 3cs2ma
title: An automation lane draws a parameter whose range spans orders of magnitude at the floor
state: todo
priority: low
created: 2026-08-26T17:03:03Z
updated: 2026-08-26T19:19:19Z
---

## What is wrong

`AutomationLanes` gives a plugin-parameter lane the range `PluginParameterInfo`
reports and maps value to y linearly across it. That was fine while every
built-in parameter crossed the facade on the engine's own bounded scale. Since
v6ac5c a built-in's value is the producer's, and one of those ranges is no
longer a comfortable span: the compressor's ratio runs from 1.05 to 1000, so a
ratio of 4 sits three tenths of one percent up the lane, indistinguishable from
1.05, and a drag anywhere in the top of the lane lands somewhere between 200 and
1000 to one.

The read itself is right — 1000 : 1 is what the engine's compressor does at the
bottom of its own range, and the range has to say so. What is wrong is drawing
it linearly.

## What to do

Give the lane a mapping from value to position that is not the identity where
the parameter's own range is not linear in the producer's ear. The engine
already holds one: `AutomatableParameter::valueRange` is a
`juce::NormalisableRange<float>` with the skew the plugin chose, and
`convertTo0to1` is the position its own editor draws at. The facade would have
to carry the position as well as the value, or carry the conversion.

Nothing outside the arrangement's automation lanes is affected: the tools carry
the number and its two ends, which is what a caller needs, and the mixer draws
no plugin parameter.

## Acceptance criteria

- [ ] A compressor ratio lane draws 4 : 1 somewhere a producer can see and hit,
      and a drag across the lane covers the ratios a producer uses.
- [ ] A parameter whose range is already linear in the producer's terms — a
      frequency in Hz, a level in dB — draws where it drew before.

## Notes

**claude** — 2026-08-26T19:19:19Z

Decision (2026-08-26, user): **an automation lane carries its target's skew as a number, and the geometry applies it.** This settles the fork in "What to do" — neither branch as worded is taken.

**Not the position.** A position is one sample of the mapping, and a lane needs the mapping in both directions: `valueAtY` must answer "what value is this y?" for any y a drag lands on, not only for the values that already have points.

**Not a conversion function.** A `std::function` across the facade breaks the vocabulary's plain-data character and makes the geometry untestable without an engine, which is exactly what `AutomationLaneTests` relies on today.

**What to build.** `PluginParameterInfo` gains a `skew` (double, default `1.0`), `AutomationLaneDrawing` gains the same, and `valueAtY` / `yForValue` apply it. The vocabulary stays plain data and the pair stays pure geometry, which is what the header in `AutomationLanes.h` promises: a gesture and the paint that follows it read the same lane the same way.

`1.0` means linear and is arithmetically what the code does today. Volume (`Mixer::faderMinimumDb..faderMaximumDb`), pan, and every parameter already linear in the producer's ear pass `1.0` and draw exactly where they drew. Criterion two is then met by construction rather than by inspection.

**The engine's skew is not the one to forward — this is the trap.** `AutomatableParameter::valueRange` describes the engine's *raw* scale. Since v6ac5c a built-in crosses the facade in the producer's units, and for the compressor's ratio those invert the raw scale: the engine holds `1 / ratio` in 0..0.95, so ascending raw is descending ratio. A skew read from the engine and applied to the real-unit range draws the lane upside down, contradicting the header's rule that a lane's top is its target's largest value.

The skew therefore belongs beside the unit conversions in the `EditOps.cpp` table that v6ac5c built. It is Duet's own statement about the producer's scale, derived once per parameter, not a number read through from the engine.

**Choose ratio's skew by measurement, not by guess.** Build a `duet_scratch` probe and pick the skew that puts 4 : 1 near the middle of the lane and keeps roughly 2 : 1 through 20 : 1 — the ratios a producer actually uses — spread across it. Record what was chosen and why in `docs/ENGINE_NOTES.md`.

Where a parameter has no sensible non-linear form, `1.0` is the right answer and needs no justification.

Build against this; no further escalation is needed on the question of how a lane maps value to position.
