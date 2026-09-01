---
id: oscfrz
title: Track I/O state and compatibility view-model
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
parent: uxkosp
created: 2026-09-01T18:33:36Z
updated: 2026-09-01T18:41:15Z
---

## Bounded implementation

Add the engine-free read/write seam that both track surfaces will use. `duet::gui::TrackIo` owns no JUCE component and exposes one snapshot per track: compatible input choices, selected input, monitoring, arm availability/state, cycle-safe outputs, and recording state.

## Settled behavior

- Input choices are `None` followed by enabled compatible devices in machine order. Audio tracks accept audio only; MIDI tracks accept MIDI only; groups accept no input and cannot arm.
- The model remembers the last known name and kind of an assigned device. If it disappears, the selected row remains as `Unavailable — <name>` until the producer chooses another input or None; it is never silently substituted.
- Assigning an incompatible/missing input or arming a track with no available input is rejected with a producer notice and no state change. Device loss disarms the affected track.
- Monitoring belongs to the selected input. With None/unavailable selected it reads While Armed but is disabled; choosing a mode in that state does nothing.
- Inputs, monitoring, and arm remain non-Action configuration. Output destinations reuse `Mixer::routingDestinations`; changing output is the existing `Set Track Output` Action.

## Acceptance and tests

- [ ] `Session` can describe an assigned input after device loss without exposing engine/JUCE types.
- [ ] `TrackIo` returns the exact choices and enabled states above for audio, MIDI, and group tracks.
- [ ] Device disappearance preserves the unavailable label, disarms, and does not change project undo or choose another input.
- [ ] Reappearing with the same device id makes the existing selection available again.
- [ ] Writes are no-ops when repeated or invalid; output writes remain one Action with digest-exact undo.
- [ ] Red/green tests live at the public model and paintless GUI seams in `tests/RecordingTests.cpp` and a focused `tests/TrackIoTests.cpp`.

Start in `modules/duet_model/include/duet/model/Session.h`, `modules/duet_model/src/Recording.cpp`, and `modules/duet_gui/include/duet/gui/`. Run the targeted tests during TDD, then all AGENTS.md build/test/format/lint checks before closing. No physical hardware review belongs in this task.
