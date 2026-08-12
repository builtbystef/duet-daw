---
id: 2z0y5u
title: 'Estimates: estimate_audio_content, the Estimate wrapper, and the estimate ledger'
state: todo
priority: medium
depends_on:
    - 3bgymu
parent: js437t
created: 2026-08-12T04:02:14Z
updated: 2026-08-12T04:02:14Z
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
