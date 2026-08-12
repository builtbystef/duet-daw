---
id: 3u1blw
title: 'CI: checks-pass gate with Debug+Release, format, tidy, and sanitizer nightlies'
state: todo
priority: high
depends_on:
    - sea14w
parent: b1j3me
created: 2026-08-11T01:50:08Z
updated: 2026-08-11T01:50:08Z
---

## What to build

Continuous integration per spec b1j3me: every push is built and tested in Debug and Release, format and lint are enforced, and one required status gates merges. Two sanitizer configurations run nightly. (The third nightly, linux-rtsan, is its own later slice.)

## Acceptance criteria

- [ ] GitHub Actions workflow on pinned `ubuntu-24.04`, with every action pinned to a full commit SHA.
- [ ] On push: configure, build, and run ctest in both Debug and Release, with no compiler cache.
- [ ] On push: `clang-format --dry-run --Werror` over Duet sources fails the run on violations.
- [ ] On push: clang-tidy runs over Duet sources only, driven by compile_commands.json — the vendored JUCE/Tracktion trees are never linted.
- [ ] A single required check named `checks-pass` aggregates all push jobs; it is the one branch-protection requirement.
- [ ] Nightly schedule runs two separate configurations, ASan+UBSan and TSan+UBSan, each building the code and running the test suite.
- [ ] A deliberately mis-formatted commit and a failing test each turn `checks-pass` red (verified once, then reverted).
