---
id: ssjy4l
title: The lint sweep is red on PluginScanDialog.cpp
state: todo
priority: high
labels:
    - maintenance
created: 2026-08-30T03:10:01Z
updated: 2026-08-30T03:10:01Z
---

## What is wrong

`./scripts/lint.sh` fails on `main`:

```
modules/duet_gui/components_src/PluginScanDialog.cpp:156:13: error: 'push_back'
is called inside a loop; consider pre-allocating the container capacity before
the loop [performance-inefficient-vector-operation,-warnings-as-errors]
```

`resultLines` fills a vector from `rows` without reserving first. The file
landed with e02ea08 (zm174o), and the sweep that missed it is the one AGENTS.md
already warns about: a sweep taken after a single-target build reads only the
files that build reached, and reads clean while the tree is not — the same trap
q1mnpd was about.

Found while sweeping for 97ynt7, whose own files lint clean. `checks-pass` is
red on `main` until this is fixed.

## Acceptance criteria

- [ ] `./scripts/lint.sh` with no arguments, after a full build, reports nothing.
