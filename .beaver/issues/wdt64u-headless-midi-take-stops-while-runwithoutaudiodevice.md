---
id: wdt64u
title: Headless MIDI take stops while runWithoutAudioDevice advances it
state: done
assignee: agent
priority: high
labels:
    - bug
created: 2026-08-24T03:18:51Z
updated: 2026-08-24T03:32:40Z
---

## Problem

The required Debug suite fails at `RecordingTests.cpp`'s `an undo during a take neither stops it nor moves the playhead`: after `runUntilThePlayheadMoves`, `session.isRecording()` is false before the test reaches the undo.

This reproduces on an untouched `origin/main`/HEAD build, so it is not caused by issue s1jzd4. It reproduces both with the ordinary environment and under `pw-jack`; changing `XDG_RUNTIME_DIR` only changes whether JUCE reports a JACK connection.

## Acceptance criteria

- [ ] `cmake --build --preset linux-debug -j 4 --target duet_tests` and `ctest --preset linux-debug --output-on-failure` pass on the development machine.
- [ ] A MIDI take driven by `runWithoutAudioDevice` remains recording until `stopRecording`, including after the playhead advances.
- [ ] The existing undo-during-take assertions remain enabled and pass.

## Notes

**agent** — 2026-08-24T03:32:40Z

Completed at the Session device seam. The hosted interface applies its MIDI list synchronously but leaves the default-device follow-up apply on the DeviceManager timer; pumping the message loop after runWithoutAudioDevice delivered that apply, freed the recording graph, and ended the take. useNoAudioDevice now cancels MIDI scanning for that hosted-only session after initialisation while restoring the production interval in PropertyStorage for future engines. The existing undo-during-take case was the red regression seam and remains enabled: recording now stays true after the playhead advances and through undo, until stopRecording. Recorded the engine fact in docs/ENGINE_NOTES.md. Checks: clang-format dry-run, full lint, duet_tests build, and ctest 168/168 passed (8 hardware cases skipped on this machine).
