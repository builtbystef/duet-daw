---
id: uaqfnv
title: 'linux-rtsan nightly: the RT-safety backstop, proven red/green'
state: todo
priority: medium
depends_on:
    - 3u1blw
    - 6zog6s
parent: b1j3me
created: 2026-08-11T01:52:13Z
updated: 2026-08-11T01:52:13Z
---

## What to build

The third nightly per spec b1j3me / ADR 0006 (nodes bd11an, ox1trt): a linux-rtsan configuration on Clang 20.1.8+ with compiler-rt — RealtimeSanitizer is driver-incompatible with ASan/TSan/UBSan/MSan, so it cannot join the existing nightlies, and the Clang 18 floor governs what builds Duet, not what lints it. Every Duet callback target and the render-test executable compile and link with the realtime sanitizer. Callback entry points — the JUCE-hosted processor's process callback and the native plugin's apply-to-buffer — are noexcept and carry the nonblocking annotation through a feature-tested project macro (the rules live in the Real-time audio section of docs/CODING_STANDARDS.md). Since no production Duet callback code exists yet, this slice creates a minimal test-only processor at each seam; offline rendering (the render harness) enters both seams with no audio device, which is what makes the nightly meaningful headlessly. The spec mandates that this first slice proves the preset red/green.

## Acceptance criteria

- [ ] A linux-rtsan CMake preset/config exists, building with Clang 20.1.8+ and the realtime sanitizer on every Duet callback target and the render-test executable; the regular GCC 13 / Clang 18 builds are unaffected.
- [ ] A feature-tested project macro applies the nonblocking annotation where supported and compiles to nothing elsewhere; both test-processor entry points are noexcept and annotated, and the sanitizer test enters through those annotations.
- [ ] A minimal test-only processor exists at each seam — one JUCE-hosted (process callback) and one engine-native (apply-to-buffer) — and both are exercised headlessly by offline render in the rtsan config.
- [ ] Red/green proof, recorded as a note on this issue: inject one heap allocation into each seam's callback → the rtsan test run fails for each; remove them → it passes. Both runs' evidence (local or CI) is linked or quoted.
- [ ] CI runs linux-rtsan as a third independent nightly job alongside the ASan+UBSan and TSan+UBSan nightlies.
- [ ] No sanitizer suppressions for Duet code; any exact upstream-function suppression carries a documented false positive (per the coding standards).
