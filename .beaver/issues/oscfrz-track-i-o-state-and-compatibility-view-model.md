---
id: oscfrz
title: Track I/O state and compatibility view-model
state: done
assignee: agent
priority: high
labels:
    - session:task
    - roadmap:yfpnps
parent: uxkosp
created: 2026-09-01T18:33:36Z
updated: 2026-09-01T20:29:32Z
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

## Notes

**agent** — 2026-09-01T20:29:32Z

Completed the engine-free Track I/O seam.

Session::assignedInput names the last known device after it leaves, without engine or JUCE types. TrackIo is paintless: None then compatible live devices, Unavailable — <name> for a lost assignment, monitoring/arm enabled only while that assignment is live, groups None-only and un-armable. Device loss disarms and does not touch undo or silently pick another input; the same id coming back restores the existing selection. Repeated or invalid input/arm/monitor writes are no-ops with a producer notice; output is the existing Set Track Output Action with digest-exact undo.

Seams: tests/RecordingTests.cpp (model) and tests/TrackIoTests.cpp (paintless GUI). Device disappearance is exercised by disabling a MIDI input, which is the headless stand-in the issue allowed (no physical-hardware review). Arrangement and Mixer chrome are 7sd7k2 and hs7owx.
