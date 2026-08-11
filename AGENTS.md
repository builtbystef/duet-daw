## Checks

- **Configure** — `cmake --preset linux-debug` (Ninja Multi-Config, exports `compile_commands.json`)
- **Build** — `cmake --build --preset linux-debug` (locally always `-j 4` — full parallelism OOM-freezes the dev machine, ~2 GB per Tracktion TU)
- **Test** — `ctest --preset linux-debug --output-on-failure` (Catch2 v3, Duet's own code)
- **Format** — `clang-format -i` over Duet sources; check with `clang-format --dry-run --Werror`
- **Lint** — `clang-tidy -p build/ <duet sources>` — never `CMAKE_CXX_CLANG_TIDY`, which would lint the vendored JUCE/Tracktion trees
- **Typecheck** — n/a; the compiler is the typechecker
- **Run** — the app is launched via `pw-jack` on the dev machine (pipewire-jack is not the system-wide libjack)

## Project docs & tracker

### Domain glossary

`docs/GLOSSARY.md` — the project's terms. Use its vocabulary in code, tests, specs, and issues. The format rules are at the top of the file.

### Coding standards

`docs/CODING_STANDARDS.md` — the conventions beyond the linter. Reviews check diffs against it.

### Architecture & decisions

`docs/ARCHITECTURE.md` — the modules and the seams. `docs/adr/` — decisions already made (the format is in `docs/adr/README.md`). Do not debate them again.

### Issue tracker

`docs/TRACKER.md` — how to use this project's issue tracker (Beaver Backlog).
