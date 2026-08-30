---
id: ssjy4l
title: The lint sweep is red on PluginScanDialog.cpp
state: done
priority: high
labels:
    - maintenance
created: 2026-08-30T03:10:01Z
updated: 2026-08-30T16:24:04Z
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

## Notes

**claude** — 2026-08-30T16:24:04Z

Fixed 2026-08-30: resultLines reserves rows.size() before the loop. Verified after a full build — full lint sweep exits 0 with no diagnostics anywhere in its output, clang-format clean, ctest 602/602.

Two things worth recording. 1qdjq5 is this same error, published independently during i84fbb while this one came out of 97ynt7; it is closed alongside this as a duplicate.

And the sweep that let it through was mine, not a loop iteration's. Finishing zm174o I ran './scripts/lint.sh 2>&1 | tail -40', which discarded everything before the last 40 lines and left the exit status unavailable — the one error I saw was the last one printed, and this one was above the cut. The trap AGENTS.md records at q1mnpd is about a partial compile database; this was a second way to get the same wrong answer, from truncating the output rather than from the database. Read the whole sweep and its exit code.
