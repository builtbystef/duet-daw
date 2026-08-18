---
id: 3u1blw
title: 'CI: checks-pass gate with Debug+Release, format, tidy, and sanitizer nightlies'
state: in-progress
assignee: claude
priority: high
depends_on:
    - sea14w
parent: b1j3me
created: 2026-08-11T01:50:08Z
updated: 2026-08-18T23:29:09Z
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

## Notes

**claude** — 2026-08-18T07:12:32Z

Facts from the skeleton slice sea14w (2026-08-18), which lands the presets this issue's CI drives:

- ONE CONFIGURE COVERS BOTH CONFIGURATIONS. CMakePresets.json defines linux-debug and linux-release as two configure presets over the same Ninja Multi-Config tree (build/), plus matching build and test presets. Debug + Release therefore need one configure and two builds, not two configures.
- COMPILE_COMMANDS.JSON HOLDS TWO ENTRIES PER SOURCE FILE — one per configuration, which is what Ninja Multi-Config emits. clang-tidy consequently lints every file twice. Harmless, but it doubles the lint job's wall clock; de-duplicate by configuration if that ever matters.
- THE CLANG TOOLS ARE VERSION-SUFFIXED. Ubuntu 24.04 ships clang-format-18 and clang-tidy-18 with no unversioned alias, so the runner must install those package names and the workflow must call the suffixed binaries, exactly as AGENTS.md now records.
- THE DUET-SOURCES SELECTOR IS `git ls-files`. The vendored JUCE and Tracktion trees live under the ignored build/, so git ls-files can never reach them — which is what keeps the format and lint jobs off the vendored code without a hand-maintained path list.
- Cold FetchContent + full Debug build measured roughly 6 minutes at -j 4 on the dev machine, inside the 4-11 min band psmj4y used to decide against a compiler cache.
