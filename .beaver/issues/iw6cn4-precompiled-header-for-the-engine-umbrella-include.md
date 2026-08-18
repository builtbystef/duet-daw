---
id: iw6cn4
title: Precompiled header for the engine umbrella include
state: done
priority: medium
labels:
    - maintenance
created: 2026-08-18T10:33:06Z
updated: 2026-08-18T11:05:00Z
---

Every Duet translation unit that reaches the engine includes `<tracktion_engine/tracktion_engine.h>`, and with it the whole JUCE and Tracktion header set. That include is what a Duet compile actually costs: roughly 2 GB of memory per TU — the reason the local build is pinned to `-j 4` — and most of the wall clock. `clang-tidy` pays the same cost a second time, because it parses the same headers to lint one Duet source.

`ccache` (root `CMakeLists.txt`, landed with 1c8sjh) removes the cost of recompiling a TU nothing has changed. It does nothing for the first compile of a source that *has* changed, which is every red-green turn, and nothing at all for the lint. A precompiled header over the engine umbrella is the remaining lever, and it is the one that also makes the memory ceiling less tight.

## Acceptance criteria

- A precompiled header carries the engine umbrella include, and every Duet target that compiles engine-facing sources uses it.
- The engine-free facade rule stays enforced by the build: `duet_tests` links only the facades and must keep compiling without the PCH, so the PCH is attached per target and never to everything.
- A clean Debug build of the whole tree is measurably faster. Record before and after in a note, measured with `CCACHE_DISABLE=1` so the number is the compiler's and not the cache's.
- `clang-tidy-18 -p build/ $(git ls-files 'modules/*.cpp' 'tests/*.cpp')` still runs and still reports nothing. A PCH changes the compile line in `compile_commands.json`, and GCC's `-include` PCH is not a format clang-tidy reads; if the two cannot share one command line, the fix is documented in `AGENTS.md`, not a lint that silently stops covering a module.
- Peak memory per TU is reported in the note alongside the timings, since `-j 4` exists because of it.
- All the checks in `AGENTS.md` pass, and every command documented there still works unchanged.

## Open while doing it

- One shared PCH or one per module.
- Whether the vendored JUCE and Tracktion module TUs are worth a PCH too: JUCE modules are INTERFACE sources, so the same header set is compiled once per consuming target.
- Whether the local `-j 4` cap can be raised afterwards, and by how much. If it can, `AGENTS.md` and the memory note both change.

## Notes

**claude** — 2026-08-18T11:05:00Z

Not doing this. The measurement says the premise is wrong.

Measured on the dev machine, 2026-08-18, warm tree:

- Rebuild `duet_tests` after a real edit to `Session.cpp` — **10.9 s**. After a revert, so ccache hits — 9.3 s.
- Full lint sweep as `AGENTS.md` documented it — **8 m 32 s**.
- Full lint sweep, Debug entries only and four at a time — **80–102 s**.
- Cold full build of the whole tree, for reference — 4 m 15 s.

This issue said the engine umbrella include is what a Duet turn costs. That holds for a cold full build and fails for the red-green loop the issue was written to speed up: compiling is about 11 s and linting is everything else. Worse, the lever cannot reach the cost. The build compiler is GCC and the linter is clang-tidy-18, and a GCC precompiled header is not a format clang can read — so a PCH would improve the 11 s and leave the 8 minutes untouched.

What actually fixed it needed no build change. `clang-tidy -p build/` was linting every file twice: Ninja Multi-Config writes one compile-commands entry per configuration, and the Release pass reports nothing the Debug pass did not. And clang-tidy is single-threaded while the files are independent. `scripts/lint.sh` filters the database to Debug and to Duet's own sources, then runs four at a time — 8 m 32 s to about 80 s. Two facts about the tooling, 6.5x.

Filtering the database by directory rather than by `git ls-files` also retires the trap that let this session's checks pass without ever having seen `Project.cpp`: an untracked source is now linted like any other.

The durable form of this is in `AGENTS.md` above the check commands, which is where it gets read before someone spends a session on the wrong lever.

A precompiled header may still earn its place one day for a reason this issue never made — cold-build and CI wall-clock, once 3u1blw lands a checks gate. That is a different justification and deserves its own issue rather than this one reopened on a premise that has been measured false.
