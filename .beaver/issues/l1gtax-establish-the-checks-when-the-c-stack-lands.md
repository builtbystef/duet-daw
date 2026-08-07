---
id: l1gtax
title: Establish the checks when the C++ stack lands
state: todo
labels:
    - maintenance
created: 2026-08-07T05:50:11Z
updated: 2026-08-07T05:50:11Z
---

There is no code and no toolchain yet, so the four checks cannot be established. When the C++ stack (build system, GUI framework, test framework) is decided and the first code lands, set up:

- **Format** — clang-format is the standard candidate; commit a `.clang-format`.
- **Lint** — clang-tidy is the standard candidate; commit a `.clang-tidy`. Start at the strictest settings the code passes.
- **Typecheck** — in C++ this is the compiler; record the build command that compiles with warnings-as-errors (or the strictest level the code passes today).
- **Test** — pick a test framework (e.g. Catch2 or GoogleTest) and add at least one smoke test so the runner passes green.

Also record the run command (how to start the app locally). Then update the Checks section in AGENTS.md (imported by CLAUDE.md) with the four commands, and write ADRs for the build system / GUI / test-framework choices — each is hard to reverse.
