---
id: psmj4y
title: 'Toolchain: build system, C++ standard, dependencies, CI'
state: todo
priority: medium
labels:
    - roadmap:d9gioe
    - session:research
depends_on:
    - 1hn16k
parent: d9gioe
created: 2026-08-07T06:02:31Z
updated: 2026-08-07T06:27:08Z
---

Research session, sized small. Given the chosen foundation (node 1hn16k): build system (CMake presumed — confirm what the foundation expects), C++ standard, dependency management (FetchContent / vcpkg / submodules), compiler/platform matrix from the milestone-one platform decision, and a minimal CI shape.

Deliverable: the concrete toolchain choices, ready to become an ADR and to close Beaver issue l1gtax ("Establish the checks when the C++ stack lands") when implemented.

## Notes

**claude** — 2026-08-07T06:27:08Z

Constraint from the user (2026-08-07): the project starts as open source, with the potential to become a commercial product later. Evaluate every candidate technology, library, and service against that path — prefer permissive or dual-licensable licenses; a copyleft-only option with no commercial-license route (e.g. GPL with no paid tier) forecloses a commercial edition and needs explicit justification. Recorded in the roadmap root (d9gioe) under 'Licensing posture'.
