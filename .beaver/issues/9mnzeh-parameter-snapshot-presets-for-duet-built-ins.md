---
id: 9mnzeh
title: Parameter-snapshot presets for Duet built-ins
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
depends_on:
    - 8ah7je
parent: jt5rjt
created: 2026-09-01T18:35:37Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Extend the existing app-global plugin preset store so parameterized Duet built-ins can use the same Save/Load chrome as external plugins without pretending they have opaque state.

## Settled format

- Preset format v2 carries device identity plus an ordered escaped list of `(parameter id, producer-unit value)`. External opaque v1 records remain readable and unchanged.
- Saving uses explicit parameter values, not the momentary automation value. Loading ignores unknown ids, clamps known values, and applies all known values as one `Load Plugin Preset` Action; an empty/no-match preset is refused with a local error and no Action.
- Identity is `builtin:eq`, `builtin:compressor`, `builtin:reverb`, or `builtin:synth`; Sampler presets remain out of scope because its zones refer to project-owned media.
- Existing name collision/replace behavior remains.

## Acceptance and tests

- [ ] Each built-in saves and reloads an independently stated parameter set as one Action with digest-exact undo.
- [ ] v1 external presets still round-trip; malformed/newer v2 data fails locally without partial application.
- [ ] Added/removed parameters make old presets forward-compatible under the ignore-unknown rule.

Start in `PluginPresets.h/.cpp` and wire the built-in window after the storage seam passes. Tests remain at preset/model seams. Run all AGENTS.md checks before closing.
