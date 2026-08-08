---
id: l1gtax
title: Establish the checks when the C++ stack lands
state: todo
labels:
    - maintenance
created: 2026-08-07T05:50:11Z
updated: 2026-08-08T08:45:40Z
---

There is no code and no toolchain yet, so the four checks cannot be established. When the C++ stack (build system, GUI framework, test framework) is decided and the first code lands, set up:

- **Format** — clang-format is the standard candidate; commit a `.clang-format`.
- **Lint** — clang-tidy is the standard candidate; commit a `.clang-tidy`. Start at the strictest settings the code passes.
- **Typecheck** — in C++ this is the compiler; record the build command that compiles with warnings-as-errors (or the strictest level the code passes today).
- **Test** — pick a test framework (e.g. Catch2 or GoogleTest) and add at least one smoke test so the runner passes green.

Also record the run command (how to start the app locally). Then update the Checks section in AGENTS.md (imported by CLAUDE.md) with the four commands, and write ADRs for the build system / GUI / test-framework choices — each is hard to reverse.

## Notes

**claude** — 2026-08-08T08:45:40Z

Node psmj4y (2026-08-08) settled the toolchain, so the checks this issue must establish are now decided. Record in AGENTS.md when the stack lands:

- Configure: cmake --preset linux-debug   (Ninja Multi-Config, CMAKE_EXPORT_COMPILE_COMMANDS=ON)
- Build:     cmake --build --preset linux-debug
- Test:      ctest --preset linux-debug --output-on-failure
- Format:    clang-format -i over Duet's own sources; CI checks with clang-format --dry-run --Werror
- Lint:      clang-tidy -p build/ <duet sources>  — NOT CMAKE_CXX_CLANG_TIDY, which would lint all of JUCE and Tracktion
- Typecheck: n/a, the compiler is the typechecker

Caveat carried from psmj4y: ctest --preset combined with --test-dir carries an 'Added in version 3.30' note in the CMake docs and our floor is 3.22. If it misbehaves, invoke ctest directly with --test-dir, which is what Tracktion itself does (it defines no testPresets at all). Full reasoning and citations are in psmj4y's closing note.
