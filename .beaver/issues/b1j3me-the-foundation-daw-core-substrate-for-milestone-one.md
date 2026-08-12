---
id: b1j3me
title: The Foundation — DAW-core substrate for milestone one
state: todo
labels:
    - spec
depends_on:
    - kimula
    - 1hn16k
    - lf8tnt
    - psmj4y
    - ddp1qt
    - hvv3nn
    - skb4tp
    - rquzdc
created: 2026-08-10T03:43:27Z
updated: 2026-08-11T01:24:09Z
---

## Problem Statement

Nothing of Duet exists yet as product code. Every milestone-one feature — and the Collaborator itself, whose spec (js437t) is already published — needs the same substrate first: a project that can be created, opened, edited, played, undone, saved, and reopened, on a stack that is proven to build and perform. The foundation area's decisions are all made and prototype-proven; they live in eight roadmap nodes and two disposable spike branches, not in a buildable application.

## Solution

A running Linux application: the Duet skeleton. The Target Producer can create a project (a folder), edit it through one edit vocabulary with reliable undo, hear edits land glitch-free during playback, save explicitly and be autosaved, recover after a crash, and load VST3 plugins scanned crash-safely out of process. The Collaborator's foundation-side obligations are met: Suggestions audition in context without entering project state, accepting one lands as a single undoable Action, and the threading contract the AI spec assumes holds by construction.

## User Stories

1. As the Target Producer, I want to create and open projects that are self-contained folders, so that a project travels with its recordings and imports.
2. As the Target Producer, I want every edit — mine or an accepted Suggestion's — to be one named undo step with full undo/redo parity, so that nothing the Collaborator does is harder to revert than my own gestures.
3. As the Target Producer, I want to edit while the transport rolls without glitches or xruns, so that editing never interrupts listening.
4. As the Target Producer, I want explicit save plus a 5-minute autosave with crash recovery, so that a plugin crash costs me minutes, not a session.
5. As the Target Producer, I want to Audition a Suggestion in context and A/B it, so that I can judge changes by ear before accepting, with zero trace if I reject.
6. As the Target Producer, I want VST3 plugins scanned in a separate process, so that one broken plugin cannot take down the app or force rescans.
7. As the Target Producer, I want a project saved by a newer Duet refused with a clear version message rather than silently damaged.

## Implementation Decisions

**Modules** (this spec creates them; ARCHITECTURE.md updated to match):

- **`duet_model`** — the edit vocabulary layer over `tracktion::engine::Edit`. Its public interface exposes **zero engine or JUCE types**: opaque `std::uint64_t` refs (wrapping `EditItemID` raw IDs), time as plain seconds/beats doubles, paths as `std::filesystem::path`. Only this module's implementation includes engine headers. This is the engine seam: a future engine swap replaces implementations, not callers.
- **`duet_persistence`** — project folder lifecycle, snapshot save, autosave, schema migration. Same facade family, same engine-free rule.
- **`duet_app`** — the JUCE GUI application shell: message loop, device management, transport, and the mount points the UI area will fill. Software renderer by default; `OpenGLContext` attachment stays the per-surface escape hatch (both proven at ddp1qt).

**The vocabulary and Actions** (from skb4tp, verbatim as proven):

- Each edit operation is expressed exactly once and always writes through the Edit's `UndoManager` — never `nullptr` (the sole exceptions: transport properties and Audition writes, below). The operation set covers the edit domains enumerated in js437t's `suggest` vocabulary (tracks, clips, MIDI notes, automation points, mixer values, plugin parameters).
- **Actions are the only transaction boundary**:

  ```cpp
  using TrackRef = std::uint64_t;   // opaque; wraps EditItemID
  using ClipRef  = std::uint64_t;

  void performAction(std::string_view name,
                     const std::function<void(EditOps&)>& ops);
  bool undo();
  bool redo();
  ```

  `performAction` calls `beginNewTransaction(name)`, runs the ops synchronously on the message thread, and deliberately does **not** seal afterwards, so the engine's deferred undo-tracked writes (the async clip re-sort, `ClipOwner`) merge into the Action that caused them. `Edit::UndoTransactionInhibitor` is held for long Actions. Raw ops are private to the Action mechanism — callable only inside `performAction` — because naked ops merge into foreign transactions or land in unnamed timer-sealed steps.
- A producer gesture and an accepted Suggestion both go through `performAction`, so undo parity and one-step collapse hold by construction. Undo depth: **200 transactions**, in-memory per session.
- Undo/redo route through `Edit::undo()/redo()`; transport properties are written with `nullptr` UndoManager so undo can never stop or reposition the transport.
- Source-file references are pinned **deliberately on every programmatic clip insertion** (never the engine default, which silently resolves to the TEMP directory): stored project-relative within the project folder.

**Suggestion and Audition** (mechanism owned here; observable behavior bound by js437t):

- A Suggestion is **data** — an ordered op list with placeholder refs for intra-Suggestion references — until accepted. Rejection discards data: zero trace in project, undo, and redo stacks. Pending Suggestions are never saved and never block saving.
- **Audition is apply-and-revert on the real Edit**: entering Audition applies the Suggestion's ops with `nullptr` UndoManager (invisible to producer undo); leaving it reverts them exactly. A/B toggle is apply/revert. Accepting first reverts the Audition, then re-applies through `performAction`.
- Revert exactness is an acceptance criterion: the canonicalized state digest after revert equals the digest before Audition (canonicalized because undo/redo and revert may permute ValueTree property order).
- Save interplay: a manual save auto-reverts a live Audition first; the autosave timer skips its tick while an Audition is live.

**Persistence** (from rquzdc, verbatim as proven):

- Duet's data lives in a **DUET child tree under `edit.state`** — one file, atomic save. Per-write undo visibility: `nullptr`-UM writes are invisible to producer undo; through-UM writes revert with it.
- **Snapshot save, never `flushState`**: copy `edit.state`, apply the parameter blobs to the copy with no UndoManager (same format as the engine's own flush, nodes located by item ID), write the copy. This preserves the redo stack even when automation has diverged parameters — the engine's own save path has this flaw; the snapshot dodges it.
- The DUET tree carries `duetSchemaVersion` (int). On load, migrations run oldest-first, one step per version, before anything reads the tree. A file newer than the app is refused, naming the needed version. The application version is stamped into the Engine's PropertyStorage.
- `EditItemID` is the durable key for all Duet-side references (proven stable across save/reload; engine reassigns only actual in-file collisions, keeping the original's ID).
- A project is a **folder**: the edit file plus an `audio/` subdirectory for recordings and imports, paths project-relative.
- **Save policy**: explicit save with dirty indication; autosave **every 5 minutes when dirty** to a single recovery file inside the project folder (never overwriting the project file), offered for restore on next open when newer than the project file.

**Thread model** (binding for all Duet code, including the Collaborator service per js437t):

- The **message thread is the sole writer of the project model** — every vocabulary op, every `performAction`, every accepted Suggestion. UI reads the model on the message thread.
- **Duet code never runs on the audio thread.** Model→graph handoff is the engine's TreeWatcher rebuild (proven inaudible under 600+ mutations during playback).
- **Worker threads** for analysis, waveform thumbnails, and offline renders (including the per-track render path js437t's tier-2 analysis needs, via the engine's Renderer). The Collaborator's socket has its own thread and marshals model reads to the message thread.
- Accepted consequence for milestone one: a very large Action briefly occupies the message thread; no background-mutation scheme exists.

**Plugin hosting** (from hvv3nn, narrowed at u24m3x):

- VST3 only, via `JUCE_PLUGINHOST_VST3=1` and the engine's `ExternalPlugin` — a switch, not a build. Out-of-process **scanning** is turned on (`EngineBehaviour::canScanPluginsOutOfProcess` → true). Hosting is in-process.

**Toolchain and CI** (from psmj4y, corrected by ddp1qt; recorded in AGENTS.md and closing `l1gtax`):

- CMake ≥ 3.22, `project(... VERSION x.y.z LANGUAGES C CXX)` (VERSION is mandatory — `juce_add_gui_app` hard-errors without it), Ninja Multi-Config, checked-in `CMakePresets.json` schema v3, `CMAKE_EXPORT_COMPILE_COMMANDS ON`.
- C++20 set by hand on every Duet target (`target_compile_features(... cxx_std_20)`) — nothing upstream enforces it. Link `atomic` explicitly (GCC lowers Tracktion's 16-byte atomics to libatomic calls).
- FetchContent, every pin a full commit SHA, **JUCE 9 declared before Tracktion** (`develop`, pinned commit), `TE_ADD_EXAMPLES OFF`. Flags: `JUCE_JACK=1`, `TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH=1`, `JUCE_USE_CURL=0`, `JUCE_WEB_BROWSER=0`, `JUCE_PLUGINHOST_LADSPA=0`.
- Linux x86_64 only; GCC 13 floor, Clang 18 secondary. Catch2 v3 for Duet's own code.
- CI: GitHub Actions on pinned `ubuntu-24.04`, Debug + Release, no compiler cache, `checks-pass` gate as the single required check, actions pinned to full SHAs, `clang-format --dry-run --Werror` on push, clang-tidy over Duet sources only via `compile_commands.json`, ASan+UBSan and TSan+UBSan as two separate nightly configs, plus the `linux-rtsan` third nightly (Testing Decisions below; amendment from ox1trt, 2026-08-10).

## Dependencies

No new dependencies beyond the set settled at psmj4y: JUCE 9, Tracktion Engine (`develop`, SHA-pinned), Catch2 v3 — all FetchContent. (ONNX Runtime and RTNeural belong to the AI-area spec's analysis slices, not the foundation.) An implementation session adds nothing else.

## Testing Decisions

**The seam under test**: the vocabulary layer's public interface (`duet_model` + `duet_persistence` facades). Headless Catch2 tests drive it exactly as the two spike scenario runners did (prior art: `prototype/undo-vocabulary` 28/28, `prototype/duet-persistence` 27/27 — disposable code, but the check lists transfer).

**What makes a good test here**: external behavior only — ops observed through reads, undo observed through canonicalized state digests (never raw XML comparison; property order permutes), persistence observed through real files and reloads. No test reaches into engine internals.

**Worked examples**:

- `performAction("Add drum loop", 5 ops)` → exactly one new undo step named "Add drum loop"; `undo()` → canonicalized digest equals the pre-action digest; `redo()` → equals the post-action digest.
- Audition enter → audible state contains the Suggestion's changes; Audition revert → canonicalized digest equals pre-Audition digest; project file on disk unchanged throughout.
- Save with automation-diverged parameter → reload restores the explicit value exactly, and the in-session redo stack survives the save.
- Load a file with `duetSchemaVersion` = current+1 → refusal naming the needed version; = current−1 → migrations run, then normal open.
- Autosave tick while dirty → recovery file appears beside the project file; project file untouched.

**RT-audio testing** (settled 2026-08-10 at roadmap nodes bd11an and xciphe, recorded at ox1trt; ADR 0006):

- **Trust boundary**: Tracktion Engine is trusted outright. Under test is only Duet's own code in the audio path — the built-in instruments and effects (the one place Duet code runs in the audio callback) and the worker-thread analysis DSP. Xrun-free mutation during playback stays prototype-verified evidence, not a regression test.
- **RT safety** ("no allocations, no locks, no syscalls in the callback") is a coding standard enforced by review — the rules are the Real-time audio section of `docs/CODING_STANDARDS.md` — with **RealtimeSanitizer as the CI backstop**: a third independent nightly config `linux-rtsan` (Clang 20.1.8+ with compiler-rt; RTSan is driver-incompatible with ASan/TSan/UBSan/MSan, so it cannot join the existing nightlies; the Clang 18 floor governs what builds Duet, not what lints it). Every Duet callback target and the render-test executable compile and link with `-fsanitize=realtime`. Callback entry points (`AudioProcessor::processBlock` for JUCE-hosted processors, `Plugin::applyToBuffer` for native ones) are `noexcept` and carry `[[clang::nonblocking]]` via a feature-tested macro; offline rendering enters both seams with no audio device, which is what makes the nightly meaningful headlessly. The first RTSan implementation slice must prove the preset red/green: inject one known allocation into each callback seam, observe the failing test, remove it, observe the pass.
- **Audio correctness** is offline render + **feature assertions with domain tolerances** — measured pitch, onset positions, RMS/spectral change — never golden files, fingerprints, or stored-sample comparison (renders proved bit-exact run-to-run on one host, but that holds within a host only and is not a contract). Known tolerance: Tracktion places note-ons at the start of their containing render block, so onset assertions allow up to one render block early. Render to a fresh destination per render (AudioFile cache), fixed sample rate/block size/bit depth, dithering off. The one sample comparison allowed is a **within-process determinism canary** in the ordinary suite: render the same Edit twice in one process, assert identical output — never against a stored file.
- **Analysis DSP** validates against synthetic reference signals only (sines at known frequencies, R128 reference levels, clicks at known positions — ground truth by construction, no licensing). Corpus benchmarking stays on the Frontier until a routine disappoints in use.
- Prior art: branch `prototype/offline-render-correctness` (both proven test patterns, 16/16), plus the trap list in xciphe's closing note.

## Out of Scope

- The producer-facing UI: timeline, piano roll, mixer, transport chrome — its own roadmap area (grill → prototype nodes follow this spec).
- Everything the milestone-one list defers: CLAP, comping, punch-in, loop-recording, recorded automation modes, pre-fader sends, out-of-process hosting, session grid.
- The Collaborator service's internals — specified at js437t; this spec only guarantees its foundation-side obligations (thread contract, Audition, one-step accept, per-track render path).
- Built-in instruments and effects beyond what the engine ships — their design is future roadmap work.

## Further Notes

Engine hazards the implementer must honor (all reproduced, sources: skb4tp/rquzdc/ddp1qt closing notes):

1. Naked ops outside an Action merge into foreign transactions or land in unnamed timer-sealed steps (350 ms timer).
2. The async clip re-sort after move/trim is undo-tracked and clears the redo stack if it fires after an undo — Actions staying open absorbs it.
3. `Edit::flushState()` writes parameter blobs through the UndoManager — never on the save path.
4. `EditFileOperations::save` segfaults for project-less edits (`EditSnapshot::refresh` null-deref, upstream bug) — Duet's save never uses it.
5. `insertWaveClip` stores TEMP-relative source paths that play silence — every insertion pins its reference.
6. The transport dies seconds after first headless playback (one-time async device-list rebuild) — headless tests retry `play()` until `isPlaying`.
7. Undo/redo permutes ValueTree property order — all comparisons canonicalize.
8. Build with `-j 4` on the dev machine (~2 GB per Tracktion TU).

Companion records published with this spec: ADR 0004 (edit vocabulary and shared undo), ADR 0005 (persistence and project shape), the ARCHITECTURE.md update, and the toolchain commands in AGENTS.md (closing `l1gtax`).

## Notes

**claude** — 2026-08-10T18:32:39Z

Amendment from UI grill s11o4w (2026-08-10): autosave becomes a Settings > Interface option (off/2/5/10 min) with DEFAULT 10 MIN, replacing this spec's fixed 5-minute interval. The recovery-file mechanism, manual-save semantics, and Audition interactions (autosave skips its tick during a live Audition) are unchanged.

**claude** — 2026-08-11T01:24:09Z

Amendment from RT-testing area record ox1trt (2026-08-10): the Testing Decisions section's 'RT testing strategy is a Frontier item' sentence is replaced with the settled doctrine — Tracktion trusted / only Duet audio code under test; RT safety as coding standard (docs/CODING_STANDARDS.md, Real-time audio section) with a linux-rtsan third nightly (Clang 20.1.8+, RTSan incompatible with the other sanitizers) proven red/green in its first slice; audio correctness as offline render + feature assertions with domain tolerances (onset up to one render block early), golden files banned, one within-process render-twice determinism canary allowed; analysis DSP validated against synthetic references only. Recorded as ADR 0006. Sources: nodes bd11an and xciphe.
