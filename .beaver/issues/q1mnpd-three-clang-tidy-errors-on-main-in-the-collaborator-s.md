---
id: q1mnpd
title: Three clang-tidy errors on main in the Collaborator's sources
state: todo
priority: medium
labels:
    - maintenance
created: 2026-08-29T01:22:32Z
updated: 2026-08-29T01:22:32Z
---

## What is wrong

`./scripts/lint.sh` over the whole tree exits non-zero on `main` (measured
2026-08-28, at 619cc18, on files this session did not change):

- `modules/duet_gui/src/CollaboratorPanel.cpp:185` — `showSuggestion`'s
  `revises` is taken by value and only read
  (`performance-unnecessary-value-param`).
- `tests/CollaboratorLoopLiveTests.cpp:214` — `harness` can be `const`
  (`misc-const-correctness`).
- `tests/CollaboratorTests.cpp:22` — the `RunStart` using-declaration is unused
  (`misc-unused-using-decls`).

Each is one line. They came in with 7tw2tz and 2suzzi, whose notes record a
clean sweep, so what is also worth a minute is why the sweep passed there —
a partial `build/tidy` compile database is the likeliest reason.

## Acceptance criteria

- [ ] `./scripts/lint.sh` with no arguments exits zero with no diagnostics.
- [ ] Nothing about what any of the three does changes.
