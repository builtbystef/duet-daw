---
id: qzjn3u
title: Sampler zone vocabulary, persistence, and playback
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
parent: jt5rjt
created: 2026-09-01T18:35:37Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Expose the engine Sampler's sounds as engine-free project data and one whole-list edit operation. Do not expose `SamplerPlugin*` or JUCE types.

## Settled zone contract

- `SamplerZoneInfo` carries name, project-relative source reference, resolved path, root note, low note, high note, and availability. Zones are addressed by their order only in a read snapshot; edits call `setSamplerZones(plugin, completeList)` so deletion/reordering cannot invalidate an exposed engine id.
- Only a Sampler plugin accepts zones. Root/low/high clamp to 0..127 and low <= root <= high. A newly loaded file defaults to name=stem, root=60 (C3), range 0..127.
- Sources must already be inside the project folder. An external/unsupported/unreadable source is rejected before mutation.
- One `Set Sampler Mapping` Action replaces the list. Engine calls use `SamplerPlugin::addSound`, `setSoundParams`, and `flushPendingUpdates`; state and project-relative references survive save/reopen and Save As.
- A missing file remains as an unavailable zone after load and is skipped/silent; valid sibling zones continue to play.

## Acceptance and tests

- [ ] Add/remap/reorder/remove whole-list edits read back exactly and undo/redo digest-exactly.
- [ ] Known MIDI renders the sample at root pitch and transposed inside its range, and silence outside it.
- [ ] Invalid plugin, external path, bad range, and missing source policies are exact and leave no partial state.
- [ ] Save/reopen and moved-project tests resolve project-owned media without engine types crossing the facade.

Start in `Session.h`, `EditOps.cpp`, `Session.cpp`, and `tests/PluginOpsTests.cpp` plus audio feature tests. Run all AGENTS.md checks before closing.
