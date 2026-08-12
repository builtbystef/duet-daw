---
id: 86t5lu
title: Write the DAW-core foundation spec
state: done
assignee: claude
priority: medium
labels:
    - roadmap:d9gioe
    - session:spec
depends_on:
    - kimula
    - 1hn16k
    - lf8tnt
    - psmj4y
    - ddp1qt
    - hvv3nn
    - skb4tp
    - rquzdc
parent: d9gioe
created: 2026-08-07T06:03:05Z
updated: 2026-08-10T03:44:47Z
---

Spec session for the foundation area: milestone scope (kimula), application foundation (1hn16k), engine layer (lf8tnt), toolchain (psmj4y), and the walking-skeleton findings (ddp1qt).

Read those nodes' closing notes, interview to close remaining gaps (likely surfaced from the Frontier: project data model, persistence, undo/redo, thread model), confirm, then run /create-specification. Record the settled choices as ADRs, and the toolchain commands in AGENTS.md (closes l1gtax).

## Notes

**claude** — 2026-08-08T08:20:18Z

Constraints from the AI-area spec (js437t, 2026-08-08), which binds behavior whose mechanisms are foundation-area work: (1) suggestion-state audition — ghost clips and ghost mixer values playable in context, A/B toggle for mix changes — needs a mechanism for entering the playback graph without entering the project state; (2) the Collaborator service's threading assumptions (socket on its own thread, project-model reads marshalled to the message thread, analysis on worker threads, nothing on the audio thread) must fit the thread model this spec settles; (3) tier-2 analysis needs a per-track offline render path through the engine.

**claude** — 2026-08-08T08:45:40Z

Constraints from node psmj4y (2026-08-08), which settled the toolchain — the foundation spec must record these:

- BUILD: CMake >= 3.22 (JUCE 9's floor; Tracktion's 3.15...3.20 does not govern), project(... LANGUAGES C CXX), Ninja Multi-Config, CMakePresets.json schema v3 checked in (CMakeUserPresets.json gitignored), CMAKE_EXPORT_COMPILE_COMMANDS ON.
- C++20, SET BY HAND. Tracktion's module headers declare no minimumCppStandard, so JUCE's module machinery gives them cxx_std_11; JUCE's own modules declare 17. The spec must state that every Duet target carries target_compile_features(... PRIVATE cxx_std_20), because nothing upstream enforces it.
- DEPENDENCIES: FetchContent, every GIT_TAG a full commit SHA, JUCE declared before Tracktion (Tracktion guards on 'if(NOT TARGET juce::juce_core)', which is how we get JUCE 9 instead of its pinned 8.0.13 and how we avoid its SSH submodule URL). TE_ADD_EXAMPLES OFF. Linked targets: tracktion::tracktion_core / tracktion_engine / tracktion_graph alongside juce:: modules directly. There is no find_package path for Tracktion — source inclusion only.
- PLATFORM: milestone one is Linux x86_64 only. Compiler floor GCC 13 / libstdc++ 13 (set by <format>, which landed at 13.1); Clang 18 on libstdc++ secondary.
- TESTS: Catch2 v3 (BSL-1.0) for Duet's own code, via FetchContent + catch_discover_tests. Note for the testing-strategy work: JUCE ships juce::UnitTest + extras/UnitTestRunner, and Tracktion's own tests are juce::UnitTest subclasses inside its module sources collected by category — so any engine-level test that exercises Tracktion's own bodies needs a SECOND, separate runner target, not a Catch2 one.
- CI: GitHub Actions, ubuntu-24.04 pinned (not -latest), Debug + Release, no compiler cache, ctest --output-on-failure, a 'checks-pass' gate job as the single required status check, actions pinned to full commit SHAs, ASan+UBSan and TSan+UBSan as two separate NIGHTLY configs (the toolchains forbid combining ASan and TSan), clang-format --dry-run --Werror on every push, clang-tidy scoped to Duet's own sources via compile_commands.json.
- The full report with citations is in psmj4y's closing note; the command set for AGENTS.md and issue l1gtax is at its end.

**claude** — 2026-08-10T03:44:47Z

RESOLVED 2026-08-10. The foundation spec is published as issue b1j3me ('The Foundation — DAW-core substrate for milestone one', label spec), with blocking edges to all eight covered nodes (kimula, 1hn16k, lf8tnt, psmj4y, ddp1qt, hvv3nn, skb4tp, rquzdc). Implementation slices come from /create-issues b1j3me — build the sub-issues, never the spec issue.

Gaps closed by interview this session (user-confirmed):
- SPEC ALTITUDE: substrate only (app skeleton, model, vocabulary, persistence, threading, save, VST3 enablement). The producer-facing UI is its own area — grill → prototype nodes added at this session's DAG extension.
- THREAD MODEL: message thread is the sole writer of the project model; Duet code never on the audio thread; workers for analysis/thumbnails/offline renders; Collaborator socket on its own thread marshalling to the message thread. Large Actions occupy the message thread — accepted for milestone one, with UndoTransactionInhibitor held for long Actions.
- AUDITION MECHANISM (the js437t-bound gap): apply-and-revert on the real Edit via nullptr-UndoManager writes; revert exactness (canonicalized digest equality) is a spec acceptance criterion; accept reverts then re-applies through performAction. Pending Suggestions are pure data — never saved, never block saving, no accept/reject prompt before save. Manual save auto-reverts a live Audition; autosave skips its tick during one.
- SAVE POLICY: explicit save with dirty indication + autosave every 5 minutes when dirty to a recovery file in the project folder (industry band is 5–15 min: FL 5/10, Reaper commonly 5, Cubase 15; 5 chosen because in-process hosting makes crashes real). Recovery offered on next open.
- UNDO DEPTH: 200 transactions (memory-trivial, KB per transaction), in-memory per session.
- TESTING STRATEGY for RT audio: stays a Frontier item; the spec records only psmj4y's settled tooling.

Companion records published this session: ADR 0004 (edit vocabulary, Actions as the only undo boundary), ADR 0005 (DUET tree, snapshot save, project folder), ARCHITECTURE.md updated to the module shape (duet_model / duet_persistence / duet_app), AGENTS.md ## Checks filled with the settled commands, issue l1gtax closed. Glossary gained Audition and Action.
