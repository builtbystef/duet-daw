---
id: 3u1blw
title: 'CI: checks-pass gate with Debug+Release, format, tidy, and sanitizer nightlies'
state: done
assignee: claude
priority: high
depends_on:
    - sea14w
parent: b1j3me
created: 2026-08-11T01:50:08Z
updated: 2026-08-19T01:18:09Z
---

## What to build

Continuous integration per spec b1j3me: every push is built and tested in Debug and Release, format and lint are enforced, and one required status gates merges. Two sanitizer configurations run nightly. (The third nightly, linux-rtsan, is its own later slice.)

## Acceptance criteria

- [ ] GitHub Actions workflow on pinned `ubuntu-24.04`, with every action pinned to a full commit SHA.
- [ ] On push: configure, build, and run ctest in both Debug and Release, with no compiler cache.
- [ ] On push: `clang-format --dry-run --Werror` over Duet sources fails the run on violations.
- [ ] On push: clang-tidy runs over Duet sources only, driven by compile_commands.json — the vendored JUCE/Tracktion trees are never linted.
- [ ] A single required check named `checks-pass` aggregates all push jobs; it is the one branch-protection requirement.
- [ ] Nightly schedule runs two separate configurations, ASan+UBSan and TSan+UBSan, each building the code and running the test suite.
- [ ] A deliberately mis-formatted commit and a failing test each turn `checks-pass` red (verified once, then reverted).

## Notes

**claude** — 2026-08-18T07:12:32Z

Facts from the skeleton slice sea14w (2026-08-18), which lands the presets this issue's CI drives:

- ONE CONFIGURE COVERS BOTH CONFIGURATIONS. CMakePresets.json defines linux-debug and linux-release as two configure presets over the same Ninja Multi-Config tree (build/), plus matching build and test presets. Debug + Release therefore need one configure and two builds, not two configures.
- COMPILE_COMMANDS.JSON HOLDS TWO ENTRIES PER SOURCE FILE — one per configuration, which is what Ninja Multi-Config emits. clang-tidy consequently lints every file twice. Harmless, but it doubles the lint job's wall clock; de-duplicate by configuration if that ever matters.
- THE CLANG TOOLS ARE VERSION-SUFFIXED. Ubuntu 24.04 ships clang-format-18 and clang-tidy-18 with no unversioned alias, so the runner must install those package names and the workflow must call the suffixed binaries, exactly as AGENTS.md now records.
- THE DUET-SOURCES SELECTOR IS `git ls-files`. The vendored JUCE and Tracktion trees live under the ignored build/, so git ls-files can never reach them — which is what keeps the format and lint jobs off the vendored code without a hand-maintained path list.
- Cold FetchContent + full Debug build measured roughly 6 minutes at -j 4 on the dev machine, inside the 4-11 min band psmj4y used to decide against a compiler cache.

**claude** — 2026-08-19T01:18:09Z

Done (2026-08-19). CI is live on GitHub Actions and every criterion is verified by a real run, not by reading the YAML.

WHAT LANDED

- .github/workflows/ci.yml — on push: `format`, `build-and-test (Debug)`, `build-and-test (Release)`, `lint`, and `checks-pass`. Every action pinned to a full commit SHA (actions/checkout fbc6f3992d24b796d5a048ff273f7fcc4a7b6c09). Runner label pinned to ubuntu-24.04.
- .github/workflows/nightly.yml — schedule 04:00 UTC plus workflow_dispatch, two matrix jobs: asan+ubsan and tsan+ubsan, each configuring, building and running the full suite.
- .github/actions/linux-build-env — one composite action holding the apt list and the build job count, so all jobs build identically.
- CMakePresets.json — `linux-asan` and `linux-tsan` configure/build/test presets. Sanitizer flags live in the preset, so a nightly report reproduces on the dev machine in one command.
- CMakeLists.txt — `DUET_CCACHE_ENABLED` (default ON). CI passes OFF.
- AGENTS.md — a CI section covering both workflows and the sanitizer commands.

VERIFIED BY RUN, NOT BY READING

- Green: run 32203210743 on main — format, lint, Debug, Release, checks-pass all success. 26 tests, of which 25 execute; 'a headless session plays through the engine's one-time device rebuild' skips itself because the runner has no audio device, exactly as the test was written to. Everything else renders offline, so headless CI needs no sound card, no JACK server and no X display.
- Red: run 32201828170 on the throwaway branch ci/verify-red, one commit carrying a mis-formatted line and a wrong expectation in tests/ProjectLayoutTests.cpp. `format` failed on the formatting; both build-and-test jobs BUILT SUCCESSFULLY and failed at the Test step on the wrong expectation; `checks-pass` failed. `lint` passed, correctly — clang-tidy has no opinion on either fault. Branch deleted afterwards; main never carried the breakage.
- Nightlies: run 32203217831 — asan+ubsan and tsan+ubsan both green.
- Branch protection on main now requires the single context `checks-pass`, with strict off and enforce_admins off, so the owner can still push directly to main (this project commits there).

DECISIONS MADE

- NO pull_request TRIGGER. A push to a branch produces these check runs and branch protection reads them on the PR, so adding the trigger would only build every branch twice. Outside contributions are out of the question (ADR 0001), so no fork ever needs a run.
- THE COMPILER CACHE IS NOW AN OPTION, NOT ONLY A SEARCH. psmj4y settled that CI builds cold; the repository auto-enabled ccache wherever it found it, which made 'no compiler cache' depend on the runner image. `-D DUET_CCACHE_ENABLED=OFF` states it instead.
- LINT CONFIGURES BUT DOES NOT BUILD. clang-tidy needs the compile database and the fetched sources, both of which exist after configure; nothing Duet compiles includes a build-generated header. Confirmed by the green run — the lint job takes 5 minutes rather than a second full build.
- THE BUILD JOB COUNT IS MEASURED, NOT WRITTEN DOWN. min(cores, RAM_GB / 2), for the same ~2 GB-per-Tracktion-TU reason AGENTS.md pins -j 4 locally. The repository was public by the time CI ran, so the runner is 4 cores / 16 GB and the number came out 4; on a 2-core/7 GB private runner it would come out 3.
- `-fno-sanitize=vptr` ON THE TSAN CONFIG ONLY. See the finding below.

TWO DEFECTS THE FIRST RUNS FOUND

1. libxi-dev was missing from the runner. juce_gui_basics includes X11/extensions/XInput2.h, which no other JUCE dependency pulls in; every juce_audio_processors TU failed to compile. The dev machine has it through something else, so only CI could find it. Fixed in 26811d9.
2. TSan+UBSan reported 23 data races on its first run, all one shape: both stacks bottom out in a pipe() call inside libubsan's IsAccessibleMemoryRange, which is how UBSan's vptr check probes an address. TSan's pipe interceptor sees libubsan's own descriptor written from two threads and calls it a race. No report named Duet, JUCE or Tracktion memory, and there were no reports of any other shape. Fixed in 75d7eea by taking vptr off the TSan configuration only — linux-asan runs UBSan with vptr on and is clean, so the check keeps its coverage and merely leaves the configuration that cannot host it. AGENTS.md records this, including the rule that a TSan report NOT ending in IsAccessibleMemoryRange is a real finding.

FACTS THE NEXT SESSION WANTS

- COST PER PUSH IS ABOUT 33 RUNNER-MINUTES on a 4-core runner: format under 1 min, lint 5, build-and-test Debug 12, Release 15, checks-pass 0. A nightly job is 19-20 min, so about 40 for the pair. Cold FetchContent plus a full build is therefore roughly twice the dev machine's 6 minutes, which is inside the 4-11 min band psmj4y used to decide against a compiler cache — the decision still holds on CI's own numbers.
- ADDING A JOB TO ci.yml MEANS ADDING IT TO `checks-pass`'s `needs` LIST. The aggregator runs `jq 'all(.[]; .result == "success")'` over toJSON(needs), so a job absent from that list cannot fail the gate. AGENTS.md says so too.
- THE SANITIZER BUILD TREES ARE build/linux-asan AND build/linux-tsan, each with its own copy of the vendored trees (about 2.2 GB of sources apiece). They do not disturb the ordinary build/ tree or scripts/lint.sh, which reads build/compile_commands.json only.
- THE THIRD NIGHTLY, linux-rtsan, IS STILL ITS OWN SLICE, as this issue's body says. It needs Clang 20.1.8+ with compiler-rt, which ubuntu-24.04 does not ship, so that slice has a toolchain question to answer that these two did not.
