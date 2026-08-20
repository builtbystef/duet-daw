## Checks

- **Configure** — `cmake --preset linux-debug` (Ninja Multi-Config, exports `compile_commands.json`). The build compiles through `ccache` when the machine has one, which is what makes a rebuild after a reverted or header-only change cheap; install it with `sudo apt install ccache`.
- **Build** — `cmake --build --preset linux-debug` (locally always `-j 4` — full parallelism OOM-freezes the dev machine, ~2 GB per Tracktion TU)
- **Test** — `ctest --preset linux-debug --output-on-failure` (Catch2 v3, Duet's own code)
- **Format** — `clang-format-18 -i $(git ls-files '*.cpp' '*.h')`; check with `clang-format-18 --dry-run --Werror $(git ls-files '*.cpp' '*.h')` (the versioned binary is what Ubuntu installs; `git ls-files` never reaches the vendored trees, which live under the ignored `build/`)
- **Lint** — `./scripts/lint.sh` — Duet's own sources, Debug configuration only, four at a time; the script's header says why each of those matters. Pass file paths to lint just those. Never `CMAKE_CXX_CLANG_TIDY`, which would lint the vendored JUCE/Tracktion trees
- **Typecheck** — n/a; the compiler is the typechecker
- **Run** — `pw-jack ./build/modules/duet_app/duet_app_artefacts/Debug/Duet` (pipewire-jack is not the system-wide libjack on the dev machine, so the wrapper is required)

### CI

`.github/workflows/ci.yml` runs the checks above on every push and aggregates
them into one required status, `checks-pass` — the only status branch protection
points at. Adding a job to that workflow means adding it to the `needs` list of
`checks-pass`, which is what makes the new job able to fail the gate.

`.github/workflows/nightly.yml` runs the two sanitizer configurations that the
push gate is too slow to carry, and both have presets, so reproducing a nightly
report locally is one command:

- **ASan + UBSan** — `cmake --preset linux-asan && cmake --build --preset linux-asan -j 4 && ctest --preset linux-asan`
- **TSan + UBSan** — the same three with `linux-tsan`

They build into `build/linux-asan` and `build/linux-tsan`, each with its own copy
of the vendored trees, so neither disturbs the ordinary build. CI configures with
`-D DUET_CCACHE_ENABLED=OFF`; leave it on locally.

On the dev machine the TSan binary refuses to start about four times in five —
`FATAL: ThreadSanitizer: unexpected memory mapping`, before any test runs, and a
core dump on some of the rest. That is this kernel's address-space randomization,
not a finding: it hits every TSan binary the same way, an untouched engine test as
readily as a new one. Run it with randomization off and it is reliable:
`setarch $(uname -m) -R ctest --preset linux-tsan`. The CI runner does not need
this. Measured on the dev machine on 2026-08-20 (issue d7h5f5).

`linux-tsan` carries `-fno-sanitize=vptr`, and that exclusion is not a shrug at a
finding. UBSan's vptr check probes an address through sanitizer_common's
`IsAccessibleMemoryRange`, which opens a pipe to do it; TSan's `pipe` interceptor
then sees libubsan's own descriptor written from two threads and reports a data
race. The first nightly run produced 23 such reports and nothing else — every one
with both stacks bottoming out in that probe, none naming Duet, JUCE or Tracktion
memory. `linux-asan` runs UBSan with vptr on and is clean, so the check is not
lost, only moved off the configuration that cannot host it. A TSan report that
does *not* end in `IsAccessibleMemoryRange` is a real finding.

### While iterating

Every check above is the one to run before a commit. Measured on the dev machine on 2026-08-18: rebuilding `duet_tests` after a real edit to `Session.cpp` takes about 11 s, and a full lint sweep about 80 s. Linting is the check worth arranging a session around; compiling is not, and a change that only speeds up the compiler is not worth much here.

- Build one target, not all of them — `cmake --build --preset linux-debug -j 4 --target duet_tests` while a test is red, `--target duet_app` while the shell is.
- Probe the engine before reading it — `cmake --build --preset linux-debug -j 4 --target duet_scratch` builds the disposable program in `tests/scratch/`. A short probe has repeatedly been cheaper than reading vendored engine sources, and the ordinary build never reaches it.
- Lint the file you changed — `./scripts/lint.sh modules/duet_model/src/Session.cpp`, a few seconds — and sweep everything once at the end.
- Format is cheap; run it whenever. It reads `git ls-files`, so `git add -N` a new file before the format check or it passes without having seen it.

## Project docs & tracker

### Domain glossary

`docs/GLOSSARY.md` — the project's terms. Use its vocabulary in code, tests, specs, and issues. The format rules are at the top of the file.

### Coding standards

`docs/CODING_STANDARDS.md` — the conventions beyond the linter. Reviews check diffs against it.

### Architecture & decisions

`docs/ARCHITECTURE.md` — the modules and the seams. `docs/adr/` — decisions already made (the format is in `docs/adr/README.md`). Do not debate them again.

### Engine notes

`docs/ENGINE_NOTES.md` — what the engine actually does. One fact per entry. The format rules are at the top of the file.

### Issue tracker

`docs/TRACKER.md` — how to use this project's issue tracker (Beaver Backlog).
