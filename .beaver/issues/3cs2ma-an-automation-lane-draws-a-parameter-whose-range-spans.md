---
id: 3cs2ma
title: An automation lane draws a parameter whose range spans orders of magnitude at the floor
state: todo
priority: low
created: 2026-08-26T17:03:03Z
updated: 2026-08-26T17:03:03Z
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
