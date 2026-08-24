---
id: q0lw4u
title: VST3 known list does not survive an Engine restart
state: done
assignee: agent
priority: high
labels:
    - bug
created: 2026-08-24T11:31:54Z
updated: 2026-08-24T13:35:39Z
---

## Problem

The app-global VST3 known list is populated by a successful scan but is empty in a fresh `Session`, so a saved hosted plugin is reported missing after restart. This independently fails two existing Debug tests:

- `a scanned VST3 joins the known list and survives a restart without a rescan`
- `a project restores a VST3's state, and still opens when the VST3 is missing`

## Acceptance criteria

- [ ] A scanned VST3 remains in `knownVst3Plugins()` after the scanning `Session` is destroyed and a fresh `Session` is constructed.
- [ ] A saved project containing that VST3 reopens with the plugin available while its bundle remains present.
- [ ] Both existing `PluginHostingTests.cpp` cases pass independently and in the full suite.

## Reproduction

On untouched `main` (the nelbwc working tree fully stashed and `duet_tests` rebuilt):

`./build/tests/Debug/duet_tests "a scanned VST3 joins*" --reporter compact`

The scan prints `Added VST3: Duet Good VST3 Fixture`, but the fresh session's known list does not contain its identifier. JUCE also asserts in `juce_TemporaryFile.cpp:115` while the scanner writes its settings, which may identify the persistence failure.

## Notes

**agent** — 2026-08-24T13:35:39Z

Completed the VST3 restart fix. The failure was the engine settings destination: the test executable inherited the producer's real JUCE config path, which is unwritable in the isolated harness, so Settings.xml never received the known list. The model now supplies a Duet PropertyStorage that resolves XDG_CONFIG_HOME at engine construction, and TestMain gives the parent plus scanner children one temporary app-global settings home. This also keeps tests from touching producer preferences. Both named PluginHostingTests pass independently; the clean q0lw4u tree builds fully, passes format and full lint, and ctest passes all 209 tests (8 hardware tests report their expected skips through the configured skip code). No interface decision changed.
