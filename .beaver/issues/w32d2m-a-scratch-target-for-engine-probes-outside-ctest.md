---
id: w32d2m
title: A scratch target for engine probes, outside ctest
state: done
assignee: agent
priority: low
labels:
    - maintenance
parent: b1j3me
created: 2026-08-19T10:56:32Z
updated: 2026-08-19T12:41:13Z
---

## Why

In both `nfjr5x` and `di0frj` the engine fact that decided the design came from
a short probe program, not from the source reading that preceded it and cost
more. `di0frj`'s measurement is the clearest case: a probe watched a take stop
at t=4.0 s while the session's input count went from 33 to 35, which named the
rebuild in one run — after `DeviceManager.cpp` had already been read twice
across two sessions for the same question.

There is nowhere to put such a program. `tests/CMakeLists.txt` lists every
source of `duet_tests` by name, and `catch_discover_tests` picks up whatever is
added, so a probe has to be added to the build, run, and then taken out again
before the commit. `sandbox/` is a container definition, not a build target.
The dance is small, but it is friction on the one move that has repeatedly been
cheaper than reading the engine, and friction there pushes the next session back
toward reading.

## What to build

A `tests/scratch/` target that builds against the same libraries `duet_tests`
does — `duet::model`, `duet::persistence`, `duet::test_support` — and is
excluded from `ctest`, so a probe never becomes a test nobody meant to keep.
Building it is opt-in: `--target duet_scratch` and nothing else builds it.

Its contents are disposable by construction. A committed probe is fine when it
records how a fact was measured; a probe that stops compiling is not a broken
build, because the ordinary build never reaches it.

## Acceptance criteria

- [ ] `cmake --build --preset linux-debug -j 4 --target duet_scratch` builds a
      runnable program that can open a `Session` and drive the engine.
- [ ] `ctest --preset linux-debug` neither builds nor runs it, and the test
      count is unchanged.
- [ ] Building `duet_tests` or `duet_app` does not build it.
- [ ] A one-paragraph note in the target's `CMakeLists.txt` says what the
      directory is for and that its contents are disposable.
- [ ] `AGENTS.md` mentions it in the "While iterating" section, as the thing to
      reach for before reading vendored engine sources.

## Notes

**agent** — 2026-08-19T12:33:29Z

Seam: the CMake/ctest commands in the acceptance criteria. This is build glue, not a Catch2 suite — a test inside duet_tests could not observe whether ctest skips the target or whether ALL builds it. Verification is those commands plus a run of the resulting binary.

**agent** — 2026-08-19T12:41:13Z

Done. tests/scratch/ is an EXCLUDE_FROM_ALL executable, duet_scratch, linking duet::model, duet::persistence, and duet::test_support. The starter opens a Session, loads the demo phrase, and plays it with no audio device.

Verified:
- cmake --build --preset linux-debug -j 4 --target duet_scratch builds; the binary prints "tracks 1, tempo 120 bpm, length 8 s, played yes" and exits 0.
- ctest --preset linux-debug still lists 71 tests and does not name scratch.
- ninja dry-run of ALL, duet_tests, and duet_app does not mention scratch.

Decisions: one main.cpp, listed by name (edit it to write a probe). No Catch2 coverage — the criteria are cmake/ctest behaviour. AGENTS.md "While iterating" now points at --target duet_scratch before reading vendored engine sources.
