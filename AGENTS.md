## Checks

- **Configure** — `cmake --preset linux-debug` (Ninja Multi-Config, exports `compile_commands.json`). The build compiles through `ccache` when the machine has one, which is what makes a rebuild after a reverted or header-only change cheap; install it with `sudo apt install ccache`.
- **Build** — `cmake --build --preset linux-debug` (locally always `-j 4` — full parallelism OOM-freezes the dev machine, ~2 GB per Tracktion TU)
- **Test** — `ctest --preset linux-debug --output-on-failure` (Catch2 v3, Duet's own code)
- **Format** — `clang-format-18 -i $(git ls-files '*.cpp' '*.h')`; check with `clang-format-18 --dry-run --Werror $(git ls-files '*.cpp' '*.h')` (the versioned binary is what Ubuntu installs; `git ls-files` never reaches the vendored trees, which live under the ignored `build/`)
- **Lint** — `./scripts/lint.sh` — Duet's own sources, Debug configuration only, four at a time; the script's header says why each of those matters. Pass file paths to lint just those. Never `CMAKE_CXX_CLANG_TIDY`, which would lint the vendored JUCE/Tracktion trees
- **Typecheck** — n/a; the compiler is the typechecker
- **Run** — `pw-jack ./build/modules/duet_app/duet_app_artefacts/Debug/Duet` (pipewire-jack is not the system-wide libjack on the dev machine, so the wrapper is required)

### While iterating

Every check above is the one to run before a commit. Measured on the dev machine on 2026-08-18: rebuilding `duet_tests` after a real edit to `Session.cpp` takes about 11 s, and a full lint sweep about 80 s. Linting is the check worth arranging a session around; compiling is not, and a change that only speeds up the compiler is not worth much here.

- Build one target, not all of them — `cmake --build --preset linux-debug -j 4 --target duet_tests` while a test is red, `--target duet_app` while the shell is.
- Lint the file you changed — `./scripts/lint.sh modules/duet_model/src/Session.cpp`, a few seconds — and sweep everything once at the end.
- Format is cheap; run it whenever. It reads `git ls-files`, so `git add -N` a new file before the format check or it passes without having seen it.

## Project docs & tracker

### Domain glossary

`docs/GLOSSARY.md` — the project's terms. Use its vocabulary in code, tests, specs, and issues. The format rules are at the top of the file.

### Coding standards

`docs/CODING_STANDARDS.md` — the conventions beyond the linter. Reviews check diffs against it.

### Architecture & decisions

`docs/ARCHITECTURE.md` — the modules and the seams. `docs/adr/` — decisions already made (the format is in `docs/adr/README.md`). Do not debate them again.

### Issue tracker

`docs/TRACKER.md` — how to use this project's issue tracker (Beaver Backlog).
