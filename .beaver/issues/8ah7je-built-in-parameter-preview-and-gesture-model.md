---
id: 8ah7je
title: Built-in parameter preview and gesture model
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

Add an engine-free `BuiltinParameterEditor` view-model over the existing parameter vocabulary and the one transient model seam it needs. No window or Sampler state belongs here.

## Settled automation and gesture policy

- `Session::previewPluginParameter` changes what is heard without an Action, dirty state, or undo; restoring the original is exact. It accepts only an existing non-Sampler parameter and clamps through the same conversion as `setPluginParameter`.
- Begin remembers the explicit value. Drag previews and displays the pending value. End restores the preview first, then commits one `Set Plugin Parameter` Action if changed; Escape/project replacement/plugin deletion restores and emits none.
- With automation and no active gesture, the control displays `automationValueAt` at the playhead and marks itself `Automated`; the explicit value remains separately available for the gesture origin. A manual commit changes the explicit value but never deletes/disarms automation, so automation remains authoritative during playback.
- Parameter snapshots preserve vocabulary order and carry id, name, range, skew, unit, display value, automation marker, and current drawing value.

## Acceptance and tests

- [ ] EQ, compressor, reverb, and 4OSC parameters round-trip preview/cancel/commit through public engine-free seams.
- [ ] A gesture is audible before commit, cancel is digest/undo neutral, and completion is exactly one Action.
- [ ] Automation movement updates drawing state without an Action; manual commit preserves curve points.
- [ ] Plugin deletion/project replacement during a gesture cannot leave a preview or dangling read.
- [ ] Tests are red/green at `Session` and paintless editor seams; audio claims use one known-parameter feature assertion.

Start in `Session.h/.cpp`, `EditOps.cpp`, and a new `duet_gui` editor model. Run all AGENTS.md checks before closing.
