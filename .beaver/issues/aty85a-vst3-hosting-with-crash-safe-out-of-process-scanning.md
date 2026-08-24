---
id: aty85a
title: VST3 hosting with crash-safe out-of-process scanning
state: done
assignee: agent
priority: medium
depends_on:
    - 4r7nlj
parent: b1j3me
created: 2026-08-11T01:51:42Z
updated: 2026-08-24T08:41:00Z
---

## What to build

VST3 plugin support per spec b1j3me (node hvv3nn, narrowed at u24m3x): hosting through the engine's external-plugin path with the VST3 host flag enabled — a switch, not a build — and scanning turned out of process, so one broken plugin cannot take down the app or force rescans of what already scanned clean. Hosting stays in-process. An inserted VST3's parameters join the existing vocabulary: one Action, one undo step.

## Acceptance criteria

- [ ] The build hosts VST3 (VST3 host flag on; out-of-process scanning enabled via the engine's scan behavior setting).
- [ ] Scanning a directory containing a known-good VST3 finds it; the found plugin appears in the known-plugins list and survives app restart without a rescan.
- [ ] Worked example (crash safety): scanning a directory that also contains a deliberately crashing plugin (a test fixture built for this purpose) completes — the scanner process dies, the app does not, the good plugin is still listed, and the crasher is marked bad so the next scan skips it.
- [ ] A scanned VST3 inserts on a track through the vocabulary and processes audio (an audible or rendered effect on the signal).
- [ ] Setting a VST3 parameter through performAction is one named undo step; read-back matches; undo restores the prior value digest-exactly.
- [ ] A project saved with a VST3 in place reloads with the plugin restored (state and parameter values), and a project referencing a now-missing plugin still opens, with the plugin flagged missing rather than the load failing.

## Notes

**claude** — 2026-08-17T04:12:26Z

Scope note (2026-08-17): the deliberately-crashing VST3 fixture is in-scope here — a minimal purpose-built plugin living in the test tree and built by the suite. The known-good fixture may be any VST3 present on the dev machine; record which one in the closing note. CI runners have no plugins installed, so CI exercises the scan-failure and empty-list paths only; the crash-recovery criterion runs where the fixture can be built and loaded.

**agent** — 2026-08-24T08:41:00Z

Built VST3 hosting and crash-safe scanning end to end.

- The shared engine configuration now enables JUCE's VST3 host, and Duet's EngineBehaviour enables and selects Tracktion's out-of-process scanner. Both the product app and the Catch executable recognise the engine's private scanner command line, so the coordinator can relaunch the current executable as its worker.
- The engine-free model facade now scans directories, reads the app-global known/bad lists, inserts a known VST3 into the producer's plugin chain, reports missing plugins, and exposes external parameters as normalised values with the plugin's own display string.
- External parameters now participate in the same Action contract as built-ins. Tracktion does not attach ExternalAutomatableParameter values to project state until a flush, so Duet states them under DUET_EXTERNAL_PARAMETERS through the Action's UndoManager and reapplies them after undo, redo, and open. The worked parameter change is one named step and undoes value- and digest-exactly.
- Persistence captures each loaded VST3's opaque state directly onto the snapshot copy, never by flushing the live Edit. Reload restores its state and parameter value; removing the bundle afterwards still permits project open and reads the plugin as missing.
- The test tree builds two VST3 fixtures: the known-good `Duet Good VST3 Fixture` (the known-good plugin used for closure) and `Duet Crashing VST3 Fixture`, whose processor constructor writes a load marker and aborts. A directory containing both finishes scanning: the good fixture remains known, the worker dies twice under Tracktion's one-retry policy, the crashing bundle is blacklisted, and a second scan does not load it again. Building the good fixture too makes the full worked examples deterministic in CI rather than limiting CI to empty/failure paths.

The six VST3 facade tests cover the flag/behaviour switches, empty and invalid directories, known-list persistence across a fresh Engine, worker crash recovery and skip, insertion/audio processing/parameter undo, and available-plus-missing project reload.

Checks: configure passed; full Debug build passed with -j 4; clang-format-18 passed; the full lint sweep passed; ctest passed 180 tests with 8 device-dependent skips (188 discovered).
