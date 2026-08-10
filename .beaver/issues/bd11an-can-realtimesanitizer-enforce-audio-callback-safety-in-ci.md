---
id: bd11an
title: Can RealtimeSanitizer enforce audio-callback safety in CI, and what is the RT-safety coding standard?
state: todo
priority: medium
labels:
    - roadmap:d9gioe
    - session:research
depends_on:
    - psmj4y
parent: d9gioe
created: 2026-08-10T23:44:00Z
updated: 2026-08-10T23:44:00Z
---

Milestone one's audio-thread code is Duet's own: two built-in instruments (synth, sampler) and three effects (EQ, compressor, reverb). The standing rule is "no allocations, no locks, no syscalls in the audio callback," enforced by coding standards + review (settled at the Frontier-sharpening session, 2026-08-10). The question: can CI enforce it too?

**RealtimeSanitizer (RTSan) landed in LLVM 20; the settled secondary compiler is Clang 18 (node psmj4y).** Find out, from primary sources and a hands-on check where possible:

- Does RTSan work on the settled toolchain, or does it need a newer Clang than the floor? A *nightly* sanitizer config may use a newer Clang than the floor compiler — the floor governs what builds Duet, not what lints it. Does RTSan compose with the existing nightly configs (ASan+UBSan, TSan+UBSan), or does it need its own config?
- What does RTSan actually catch in a JUCE/Tracktion host — does instrumenting Duet's processor callbacks work when the callback is invoked from an uninstrumented engine? How are `[[clang::nonblocking]]` annotations applied at the `processBlock` seam?
- False positives / suppression story for the vendored trees.
- Whether a headless render (no real audio device) exercises the callback path enough for RTSan to be meaningful in CI.

**Deliverable is double** (settled at the sharpening session): (1) the feasibility answer — if RTSan works, the concrete nightly-config addition; if not, the reason and what the fallback is (debug-build assertion hooks were option (c), discipline-only is the floor). (2) The RT-safety section of `docs/CODING_STANDARDS.md` — what audio-callback code may not do — written by this session, since it will have just enumerated exactly what RTSan does and does not catch.

Trust boundary (settled): Tracktion Engine is trusted outright; only Duet's own code is in scope.
