---
id: 1mldqz
title: The engine's wet/dry parameters on a hosted plugin cross as the vendor's own
state: done
assignee: claude
priority: medium
labels:
    - bug
parent: js437t
created: 2026-08-30T02:57:41Z
updated: 2026-08-30T18:01:46Z
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

## Notes

**claude** — 2026-08-30T16:35:35Z

Seams for this work: the protocol seam (`ToolRun` through `tests/ProjectToolsHarness.h`) for every acceptance criterion — get_plugin_chain's shape, the ledger, and the `suggest` write side — with `Session` in tests/PluginHostingTests.cpp for what the facade reads off a hosted plugin. That is the outermost seam that can observe all three criteria, and the one spec js437t names first.

**claude** — 2026-08-30T18:01:46Z

Built. The decision the body left open is taken here, and it is the first of the
two shapes it named.

## THE DECISION

**The engine's dry and wet levels are reported, bare, with a unit — a level in
decibels, silence to unity — exactly as a built-in's parameters are.** They are
not omitted, and they are not the vendor's.

Three things in the repository decided it rather than taste:

- The producer can already reach them. `AutomationLanes` offers every parameter
  of every plugin on a track as a lane target, so "Fixture: Wet Level" is a
  curve the producer can draw today. Omitting the pair would put the Collaborator
  outside the closure principle in the wrong direction — unable to name an edit
  the producer can make.
- `get_automation` would otherwise contradict `get_plugin_chain`. A lane on a
  hosted plugin's wet level reports a `pluginParam` target, and a chain that
  omitted the parameter would leave the model a target it cannot resolve.
- Decibels, not the engine's 0..1 gain, because the engine's own display text
  for these is decibels and Duet's built-in reverb already crosses its
  identically-named `wet level` / `dry level` in decibels (v6ac5c). Two scales
  for one word would be the confusion that issue removed, in a second place.

## WHAT LANDED

- `PluginParameterInfo::duetOwnsMeaning` is the facade's statement of whose
  meaning a parameter carries, and it is what `get_plugin_chain` branches on —
  no longer whether the plugin is a built-in. The engine's two are told apart by
  identity (`ExternalPlugin::dryGain` / `wetGain`, both public), never by name,
  which a vendor is free to imitate.
- `unitsOfParameter` (was the private `unitsOf`) answers `decibelsFromGain` for
  those two. Because `realParameterValue`, `engineParameterValue` and
  `parameterSkew` all go through it, one edit puts the read, the write
  (`setPluginParameter`), and both ends of an automation curve on the same
  scale — v6ac5c's "one scale per parameter, wherever it is read".
- `SuggestTool::parameterRange` now takes the ends the read side reported for
  any plugin the project holds, rather than forcing 0..1 for every external. For
  a plugin an element is still only adding, `duet::model::hostedDryLevelParameterId`
  and `hostedWetLevelParameterId` are known before the plugin exists, so those
  two are held to decibels there too. Without that, a Suggestion writing
  `wet level: 0.5` against a plugin it was adding would have been accepted and
  applied as 0.5 dB — clamped to unity, silently, which is exactly the failure
  v6ac5c's decision exists to prevent.
- The estimate ledger takes a line only for a value that crosses wrapped, and
  the engine's two no longer do. A run handed nothing but them is handed no
  guess and is not marked.
- Prose that claimed the old contract is corrected where it stood:
  `PluginParameterInfo`, `Session::setPluginParameter`, `ProjectTools.h`,
  `SuggestTool.h`, and the model-facing descriptions of `get_plugin_chain` and
  `plugin.setParam` in `sidecar/src/vocabulary.ts`.

## FACTS FOR A REVIEWER

- **Criterion two is asserted on the ledger, not on a plugin.** A hosted plugin
  whose whole parameter list is the engine's two cannot be made out of a fixture:
  JUCE's VST3 wrapper gives a processor with no parameters a "Bypass" of its
  own, so the good fixture reports four parameters — Dry Level, Wet Level, Gain,
  Bypass. The ledger holds one line per wrapped value, so "no line for them" is
  the whole of the claim, and the test asserts the entries are exactly the
  vendor's and name neither engine id. Such a plugin is real in the world, just
  not buildable here.
- **Nothing on disk changed meaning.** Curves still store the engine's gain and
  the plugin state is untouched; only the numbers the facade speaks moved. No
  project written before this reads differently.
- **The constants are pinned by a test.** The add-path range check matches on
  the ids `dry level` / `wet level`, which is a string match where the runtime
  path uses identity, so `PluginHostingTests` asserts a real hosted plugin
  reports parameters under exactly those ids and that they are Duet's.
- Three existing tests took the first parameter of a hosted plugin's list and so
  were about the engine's dry level without meaning to be; each now finds the
  parameter it is about by whose meaning it carries.
- Recorded in `docs/ENGINE_NOTES.md`, in the entry 97ynt7 opened.
