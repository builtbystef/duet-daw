---
id: 1mldqz
title: The engine's wet/dry parameters on a hosted plugin cross as the vendor's own
state: todo
priority: medium
labels:
    - bug
parent: js437t
created: 2026-08-30T02:57:41Z
updated: 2026-08-30T02:57:41Z
---

## What is wrong

Tracktion gives every hosted external plugin two automatable parameters of its
own — "Dry Level" and "Wet Level" — and `get_plugin_chain` cannot tell them from
the vendor's. So they cross the seam in the external shape: `vendorName`,
`normalizedValue`, and a `displayString` wrapped as an estimate whose method
says "the plugin's own display text, unaltered".

Two things about that are untrue. The name is the engine's, not the vendor's;
and the display text comes from `PluginWetDryAutomatableParam::valueToString`,
which is Duet's own dependency and not the plugin. Duet owns what those two mean,
so by ADR 0002 they should cross bare, with a unit, exactly as a built-in's do —
and they should not write a line into the run's estimate ledger, which they do
today. A run that read a hosted plugin is marked as based on estimates even if
the only wrapped values it was handed were these two.

Found while building 97ynt7, whose worked example had to look the vendor's
parameter up by name rather than take the first one.

## What to decide, then build

Whether the engine's wet/dry pair is reported at all, and if so in which shape.
Reporting them bare with a unit keeps `plugin.setParam` able to reach them and
keeps the provenance honest; omitting them keeps the chain to what the vendor
declares and takes them out of the vocabulary's reach.

## Acceptance criteria

- [ ] A hosted VST3's chain entry distinguishes the engine's own parameters from
      the vendor's, in whichever of the two shapes the decision names.
- [ ] A run whose only plugin read was of the engine's own parameters is not
      marked as based on estimates, and its ledger holds no line for them.
- [ ] Reading a hosted plugin's vendor parameters still marks the run, as 97ynt7
      asserts.
