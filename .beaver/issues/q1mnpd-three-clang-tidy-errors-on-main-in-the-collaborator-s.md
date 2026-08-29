---
id: q1mnpd
title: Three clang-tidy errors on main in the Collaborator's sources
state: todo
priority: medium
labels:
    - maintenance
created: 2026-08-29T01:22:32Z
updated: 2026-08-29T07:04:59Z
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

## Notes

**claude** — 2026-08-29T06:22:13Z

Progress (2026-08-29): the second of the three — tests/CollaboratorLoopLiveTests.cpp, 'harness' can be const — is fixed in the working tree alongside the live-loop test work for 2suzzi, because that change already had the file open. The other two are untouched. Worth keeping this issue's own observation in view: 7tw2tz and 2suzzi both recorded a clean lint sweep while main fails, so an iteration's self-reported 'lint clean' is not evidence.

**claude** — 2026-08-29T07:04:59Z

Update (2026-08-29). Fixed: showSuggestion's `revises` now taken by const
reference; CollaboratorLoopLiveTests' `harness` is const;
CollaboratorTests' unused `RunStart` using-declaration removed. All three this
issue names are done.

It stays open because a sweep of the whole tree finds two more, both in
tests/SuggestionSurfacesTests.cpp (:371 and :373): `ghostFader()` is called once
to check `has_value()` and a second time to read through `->`, so the read is
unguarded as far as bugprone-unchecked-optional-access can tell. Binding the
optional to a local once fixes both. They came in with 2suzzi and are not
regressions from the work above.

Worth recording that this issue's own hypothesis is what happened: a sweep is
only as wide as the compile database it runs against, so one taken after a
partial build reports fewer errors than the tree holds. The count grew here
after a full build, not because anything got worse.
