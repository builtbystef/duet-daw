---
id: ox1trt
title: RT-audio testing settled — record the area's testing decisions
state: done
assignee: claude
priority: medium
labels:
    - roadmap:d9gioe
    - session:spec
depends_on:
    - bd11an
    - xciphe
parent: d9gioe
created: 2026-08-10T23:44:26Z
updated: 2026-08-11T01:24:34Z
---

The RT-audio testing area is bd11an (RTSan feasibility + the RT-safety coding standard) and xciphe (offline-render determinism + feature-assertion doctrine). When both close, their answers are what an implementer of audio code needs together.

This area is small and its natural home already exists: b1j3me's Testing Decisions section (which deferred RT testing to the Frontier) and docs/CODING_STANDARDS.md (which bd11an's session writes directly). Read the two closing notes, interview to close any gap, then decide with the user whether the record is an amendment to b1j3me's Testing Decisions plus the standards section — the likely right answer for two nodes — or a standalone spec via /create-specification. Either way, the record must name: the trust boundary (Tracktion trusted), the enforcement mechanism (RTSan config or its fallback), the correctness doctrine (offline render + feature assertions, with the determinism tolerance xciphe measured), and the synthetic-only ground-truth rule for analysis DSP.

## Notes

**claude** — 2026-08-11T01:24:30Z

## Record — 2026-08-10

The RT-audio testing area (bd11an + xciphe) is recorded, as the node body predicted, as an amendment rather than a standalone spec — the area is two nodes and both natural homes existed. Confirmed with the user, all three recommendations accepted.

**Where the record lives:**

1. **ADR 0006** (docs/adr/0006-audio-testing-feature-assertions-rtsan.md) — the hard-to-reverse doctrine: Tracktion trusted / only Duet audio code under test; correctness by feature assertions on offline renders, never golden files (bit-exactness held on one host but is deliberately not relied on); RTSan as dynamic backstop, review as the enforcement.
2. **b1j3me Testing Decisions, amended** (note on b1j3me, 2026-08-10) — the implementer-facing detail: linux-rtsan third nightly (Clang 20.1.8+, driver-incompatible with ASan/TSan/UBSan/MSan; the Clang 18 floor governs builds, not lints), -fsanitize=realtime on Duet callback targets + render-test executable, noexcept + [[clang::nonblocking]] via feature-tested macro on processBlock/applyToBuffer, red/green proving requirement for the first RTSan slice, feature-assertion tolerances (onset up to one render block early), fresh render destination per render, synthetic-only ground truth for analysis DSP, prior art branch prototype/offline-render-correctness.
3. **docs/CODING_STANDARDS.md, Real-time audio section** — written directly by bd11an's session; unchanged here, confirmed as the standing rule set.

**Decisions taken at this session (user-confirmed):** (a) amendment over standalone spec; (b) one short ADR (0006); (c) a within-process render-twice determinism canary is kept in the ordinary test suite — same Edit rendered twice in one process must be identical; never compared against a stored file, so the golden-file ban stands.

**Reason:** a standalone spec would only have duplicated two closing notes; the amendment puts each fact where its implementer will look (CI shape in the foundation spec, callback rules in the coding standards, the trade-off in an ADR).
