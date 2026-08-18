## Checks

- **Configure** — `cmake --preset linux-debug` (Ninja Multi-Config, exports `compile_commands.json`). The build compiles through `ccache` when the machine has one, which is what makes a rebuild after a reverted or header-only change cheap; install it with `sudo apt install ccache`.
- **Build** — `cmake --build --preset linux-debug` (locally always `-j 4` — full parallelism OOM-freezes the dev machine, ~2 GB per Tracktion TU)
- **Test** — `ctest --preset linux-debug --output-on-failure` (Catch2 v3, Duet's own code)
- **Format** — `clang-format-18 -i $(git ls-files '*.cpp' '*.h')`; check with `clang-format-18 --dry-run --Werror $(git ls-files '*.cpp' '*.h')` (the versioned binary is what Ubuntu installs; `git ls-files` never reaches the vendored trees, which live under the ignored `build/`)
- **Lint** — `clang-tidy-18 -p build/ $(git ls-files 'modules/*.cpp' 'tests/*.cpp')` — never `CMAKE_CXX_CLANG_TIDY`, which would lint the vendored JUCE/Tracktion trees
- **Typecheck** — n/a; the compiler is the typechecker
- **Run** — `pw-jack ./build/modules/duet_app/duet_app_artefacts/Debug/Duet` (pipewire-jack is not the system-wide libjack on the dev machine, so the wrapper is required)

### While iterating

Every check above is the one to run before a commit. Compiling and linting a Duet source both cost the whole JUCE and Tracktion header set, so running the full set on every red-green turn is the single biggest waste of a session:

- Build one target, not all of them — `cmake --build --preset linux-debug -j 4 --target duet_tests` while a test is red, `--target duet_app` while the shell is.
- Lint the files you changed — `clang-tidy-18 -p build/ modules/duet_model/src/Session.cpp` — and sweep everything once at the end. A file the tree does not track yet is invisible to `git ls-files`, so `git add -N` new files before that sweep or the checks pass without having seen them.
- Format is cheap; run it whenever.

## Project docs & tracker

### Domain glossary

`docs/GLOSSARY.md` — the project's terms. Use its vocabulary in code, tests, specs, and issues. The format rules are at the top of the file.

### Coding standards

`docs/CODING_STANDARDS.md` — the conventions beyond the linter. Reviews check diffs against it.

### Architecture & decisions

`docs/ARCHITECTURE.md` — the modules and the seams. `docs/adr/` — decisions already made (the format is in `docs/adr/README.md`). Do not debate them again.

### Issue tracker

`docs/TRACKER.md` — how to use this project's issue tracker (Beaver Backlog).
