---
id: 2z0y5u
title: 'Estimates: estimate_audio_content, the Estimate wrapper, and the estimate ledger'
state: done
assignee: claude
priority: medium
depends_on:
    - 3bgymu
parent: js437t
created: 2026-08-12T04:02:14Z
updated: 2026-08-28T21:34:58Z
---

## What to build

Tier 3, and the provenance machinery that keeps a guess from ever passing as a fact. The estimating tool answers key and chords for a track and optional bar range, from chroma features scored by Krumhansl-Schmuckler over that track's rendered output. Every estimated value crosses the seam wrapped, carrying its method and a confidence between 0 and 1; nothing read or measured is ever wrapped. That asymmetry is the whole provenance contract (ADR 0002).

The service keeps a per-Task-Run estimate ledger of every wrapped value handed to the model. Once the ledger is non-empty, that run's Suggestion and all its subsequent commentary are marked as based on estimates, and the ledger is retrievable so the mark can be inspected. The marking is mechanical taint, never the model's self-report — over-marking is accepted, and narrowing it later is a presentation change. The system prompt's hedging instruction is additional and never the mechanism.

## Acceptance criteria

- [ ] Key, worked: a rendered C-major triad progression returns key as a wrapper — never a bare value — whose value is "C major", whose method names the routine, and whose confidence lies in 0..1 and exceeds the same routine's confidence on white noise.
- [ ] Chords, worked: a rendered two-bar progression of C major then G major returns a wrapped chord list naming those chords at bars 1 and 2.
- [ ] The aspects argument restricts the work: asking only for key does not compute chords, and an unrequested aspect is absent from the result rather than empty.
- [ ] Across the fixture corpus, nothing this tool returns is ever bare and nothing the project-read or measured tools return is ever wrapped.
- [ ] Ledger, worked: a run that calls this tool and then produces commentary has that commentary marked as based on estimates; a run that calls only project-read and measured tools produces commentary with no mark.
- [ ] Once tainted, a run stays tainted: every later result of that run carries the mark even when the estimate went unused, and the mark never depends on anything the model says about itself.
- [ ] A run's ledger is retrievable and names each estimated value handed over, with its method and its confidence.
- [ ] Each new run starts with an empty ledger; a previous run's taint never leaks into it, including after a canceled or failed run.
- [ ] Estimation runs on a worker thread and is cached and invalidated on the same terms as the measured analysis.

## Notes

**claude** — 2026-08-28T21:34:58Z

Built 2026-08-28. Every acceptance criterion is met and asserted. Two things to
know before reading the diff: what the corpus sweep renders, and that the model
is now offered two aspects where the contract names four.

## WHAT LANDED

- `modules/duet_collab/src/Harmony.cpp` — tier 3 as pure functions of a
  waveform, in the target that links nothing at all (`duet::collab_analysis`):
  `analysis::estimatedKey` and `analysis::estimatedChord`, each answering a name
  and a confidence, beside `keyMethod` and `chordMethod`, which is how a routine
  names itself in the `method` of a wrapped value. Both read a pitch-class
  profile off the averaged power spectrum and correlate it — the
  Krumhansl-Schmuckler key profiles for the one, major and minor triad templates
  for the other. `src/Spectrum.h` declares that transform now, so the bands and
  the pitch classes are two readings of one of it.
- `include/duet/collab/Estimate.h` and `src/Estimate.cpp` — `Estimate`,
  `wrapped()` and `EstimateLedger`, in `duet::collab`, which links only the JSON
  the seam speaks.
- `include/duet/collab/ContentEstimates.h` and `src/ContentEstimates.cpp` —
  `estimate_audio_content` in `duet::collab_tools`.
- `include/duet/collab/TrackRenders.h` and `src/TrackRenders.cpp` — the
  per-track render, taken out of `TrackAnalysis` unchanged so that both analysis
  tools share one; `src/AnalysisCall.h` and `.cpp` — what a call naming a track
  and a stretch of it means, shared for the same reason, and `barCountOf`, which
  `ProjectTools` had a copy of.
- The mark: `CollaboratorService::setEstimateLedger`,
  `TaskRunListener::commentaryDelta` carrying it, and
  `Suggestion::basedOnEstimates`, stamped by `SuggestTool` out of the ledger.
- `sidecar/src/vocabulary.ts` — the tool's description, and its `aspects`
  narrowed to what this build answers.
- Tests: `tests/HarmonyRoutinesTests.cpp` (6 cases), `tests/ContentEstimateTests.cpp`
  (16), a corpus sweep in `ToolFixtureTests.cpp`, `isAnEstimate` and
  `holdsAnEstimate` shared through `ProjectToolsHarness.h`, and
  `TempProject::writeChords`, which writes the progression a fixture renders.
- Docs: the AI bullet of `ARCHITECTURE.md`; `Estimate` and `Estimate Ledger` in
  `GLOSSARY.md`.

## DECISIONS A REVIEWER NEEDS

1. **Wrapping and recording are one act.** `EstimateLedger::record` is what
   answers with the wrapped value, so nothing can hand an estimate over without
   the run's ledger holding it. That is the whole of "mechanical taint, never
   the model's self-report": the service reads the ledger as each `run.text`
   arrives and marks the delta, and the model has no path to that bit at all.

2. **The ledger is given to the service rather than owned by it.** The
   estimating tool needs it at construction and the service is constructed
   beside the tools, so the wiring layer owns it: `setEstimateLedger` registers
   it and `startRun` empties that run's lines. A service with no ledger marks
   nothing, which is what a Collaborator with nothing estimating wired to it
   should say.

3. **One render store for both tools.** `TrackRenders` is the tier-2 cache,
   behaviour unchanged, moved out of `TrackAnalysis` and shared, so a track
   measured and estimated in one run is rendered once and both answers come out
   of that render. `TrackAnalysis`'s constructor taking it is the only churn in
   an existing suite.

4. **The model is offered what this build can answer.** The contract names four
   aspects and this build estimates two. Rather than answer `notes` with
   silence, the sidecar's `aspects` union is `key` and `chords`, a test holds
   the schema to that, and bmrxnw carries a note to put the other two back
   beside the transcription that answers them. A call asking only for an aspect
   this build cannot estimate is an error the model can correct against.

5. **Two chords do not name a key, and the routine says so.** A rendered C then
   G reads as "E minor", which is a fair reading of those six pitch classes, so
   the key criterion's fixture is a whole I–IV–V–I. What the confidence is for
   is exactly this: 0.93 for that cadence against 0.31 for white noise, measured
   on the dev machine.

6. **A bar's chord is read from the middle of the bar.** The ends of a bar hold
   the chord before it still ringing and the next one already struck — the
   engine starts a sound at the beginning of the block that holds it — so a
   twentieth of the bar is left out at each end.

7. **What cannot be read is not answered.** A silent track has no key and no
   chords, and what comes back is `{}` rather than the key silence fits least
   badly. An aspect nobody asked for is absent for the same reason it is not
   computed.

8. **The corpus sweep renders a stand-in.** All seven fixtures go through both
   analysis tools and all five read tools, with the analysis reading a signal
   the suite wrote instead of the project's own render. Measured on the dev
   machine 2026-08-28: a real render of one of these ninety-six-bar projects
   costs about 50 s in an unoptimised Debug build — longer than the 20 s a Task
   Run waits in the harness, so the run times out — and would say nothing more
   about provenance for it, where the stand-in puts all seven through in 10 s.
   What a real render measures and estimates is asserted over real projects in
   `TrackAnalysisTests` and `ContentEstimateTests`.

9. **"Asking only for key does not compute chords" is asserted as absence.** A
   key-only result has no `chords` member, and the routine that would compute it
   sits inside the branch that was not taken. Between "not computed" and
   "computed and dropped" there is no observable at the seam that is not a
   stopwatch.

10. **`displayString` is still not in the ledger.** The other wrapped value the
    spec names is a scanned plugin's own display text. It goes through the same
    `wrapped()` now, but nothing records it, so a run whose only guess was a
    plugin's text is not marked. That is 97ynt7's own acceptance criterion, and
    it carries a note saying where the mechanism is.

## CHECKS

- Format: `clang-format-18 --dry-run --Werror` over `git ls-files` — clean.
- Lint: `./scripts/lint.sh` over every file this diff touches — clean. Two
  `cert-err58-cpp` errors on file-scope test fixtures were fixed rather than
  silenced.
- Sidecar typecheck: `bun run typecheck` — clean.
- Build: `cmake --build --preset linux-debug -j 4`, every target — clean.
- Test: `ctest --preset linux-debug` — 454/454, of which 23 are this slice's own.
