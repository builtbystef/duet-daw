---
id: ox1trt
title: RT-audio testing settled — record the area's testing decisions
state: todo
priority: medium
labels:
    - roadmap:d9gioe
    - session:spec
depends_on:
    - bd11an
    - xciphe
parent: d9gioe
created: 2026-08-10T23:44:26Z
updated: 2026-08-10T23:44:26Z
---

The RT-audio testing area is bd11an (RTSan feasibility + the RT-safety coding standard) and xciphe (offline-render determinism + feature-assertion doctrine). When both close, their answers are what an implementer of audio code needs together.

This area is small and its natural home already exists: b1j3me's Testing Decisions section (which deferred RT testing to the Frontier) and docs/CODING_STANDARDS.md (which bd11an's session writes directly). Read the two closing notes, interview to close any gap, then decide with the user whether the record is an amendment to b1j3me's Testing Decisions plus the standards section — the likely right answer for two nodes — or a standalone spec via /create-specification. Either way, the record must name: the trust boundary (Tracktion trusted), the enforcement mechanism (RTSan config or its fallback), the correctness doctrine (offline render + feature assertions, with the determinism tolerance xciphe measured), and the synthetic-only ground-truth rule for analysis DSP.
