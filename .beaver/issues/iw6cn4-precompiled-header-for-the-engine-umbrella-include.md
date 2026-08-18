---
id: iw6cn4
title: Precompiled header for the engine umbrella include
state: todo
priority: medium
labels:
    - maintenance
created: 2026-08-18T10:33:06Z
updated: 2026-08-18T10:33:06Z
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
