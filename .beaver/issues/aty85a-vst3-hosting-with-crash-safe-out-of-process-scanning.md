---
id: aty85a
title: VST3 hosting with crash-safe out-of-process scanning
state: todo
priority: medium
depends_on:
    - 4r7nlj
parent: b1j3me
created: 2026-08-11T01:51:42Z
updated: 2026-08-17T04:12:26Z
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
