---
id: 1qdjq5
title: resultLines pushes into a vector in a loop, and the sweep calls it an error
state: todo
priority: low
labels:
    - maintenance
created: 2026-08-30T04:37:11Z
updated: 2026-08-30T04:37:11Z
---

## What happens

`./scripts/lint.sh` reports one error, in a file no open issue is touching:

```
modules/duet_gui/components_src/PluginScanDialog.cpp:156:13: error: 'push_back' is called
inside a loop; consider pre-allocating the container capacity before the loop
[performance-inefficient-vector-operation,-warnings-as-errors]
```

`PluginScanPanel::resultLines()` builds its vector row by row with no `reserve`,
which is what the check asks for.

## Where it came from

The file has not changed since zm174o landed it (commit e02ea08), so the sweep
that slice ran either did not reach this file or was taken before the method was
written — the trap AGENTS.md records at q1mnpd: a sweep taken after a
single-target build reports only the files that build reached and reads as clean
while the tree is not.

Found during i84fbb, whose full sweep was the first to cover the file again.
Nothing about it is i84fbb's, so it is here rather than in that diff.

## Acceptance criteria

- [ ] `./scripts/lint.sh` reports no error for `PluginScanDialog.cpp`.
- [ ] The full sweep, taken after a full build, is clean.
