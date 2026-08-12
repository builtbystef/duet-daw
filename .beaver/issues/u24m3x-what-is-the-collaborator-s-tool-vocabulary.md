---
id: u24m3x
title: What is the Collaborator's tool vocabulary?
state: done
assignee: claude
priority: high
labels:
    - roadmap:d9gioe
    - session:grill
parent: d9gioe
created: 2026-08-08T03:35:57Z
updated: 2026-08-08T07:15:38Z
---

Grill session. Node lxwoas chose the full Collaborator shape for milestone one, so the perception-side vocabulary must cover every domain the closure principle covers. Settle, as the mirror of hll1mo's edit vocabulary:

- Which deterministic analyses the model can call, and what each returns (the tool contracts / schemas).
- The closure principle that says the set is sufficient — the perception-side analogue of "a Suggestion may contain exactly what the producer could do through the UI". Candidate phrasing to test in the interview: the model may read anything the producer can see or hear-as-a-measurement in the UI.
- Which analyses are computed versus read straight off the engine's Edit ValueTree.

Inputs to read first:

- fod077's validated five-tool draft (list_tracks, get_arrangement, get_track_analysis, get_midi, get_plugin_chain) and its fixture schemas — explicitly named the first draft of this vocabulary.
- lf8tnt's finding: Tracktion's level measurement is a runtime meter; no offline deterministic per-track loudness API was found, so measured loudness likely must be built.
- sdfjqh's analysis-library licence findings for the deterministic layer: Beat This (MIT code and weights) and Basic Pitch (Apache-2.0) are clean; madmom's models are CC-BY-NC-SA-4.0 and Essentia is AGPL-3.0 with CC-BY-NC-ND models — both landmines under the licensing posture.

Feasibility facts (what a library can measure, under which licence) are found in-session; the decisions are the user's. Deliverable: the tool list with contracts and the sufficiency principle, ready for the AI-area spec (o3mgk1).

## Notes

**claude** — 2026-08-08T07:15:19Z

Grill session settled (2026-08-08). The Collaborator's Tool Vocabulary.

## The sufficiency principle — provenance, not UI parity

The node's candidate phrasing ("the model may read anything the producer can see or hear-as-a-measurement in the UI") was rejected in the interview: it tests the wrong thing. A producer cannot see integrated LUFS unless a meter is open, so UI parity either bans measurements the fod077 prototype proved essential (crest factor WAS the whole fixture-d diagnosis) or gets stretched until it means nothing.

SETTLED PRINCIPLE: every fact the Collaborator receives is read from the project data model, measured from rendered audio by a documented routine, or explicitly marked as an estimate. Nothing arrives unlabelled that could be wrong.

The three tiers this rests on:

- TIER 1 — the project already knows it, no analysis at all: tempo and the tempo map, time signature, bar/beat positions, clip boundaries, MIDI notes, every mixer and plugin value. A DAW is TOLD its tempo; it never detects it. This tier is larger than a standalone analyser's world and it is exact.
- TIER 2 — measured, classic DSP, genuinely a property of the waveform: RMS, peak, true peak, integrated and short-term LUFS (ITU-R BS.1770 is a written spec, so deterministic by construction), crest factor, spectral band energy, centroid, flatness, stereo correlation and width, onset times via spectral flux, monophonic f0 via YIN/autocorrelation.
- TIER 3 — estimates, ML or not: key, chords, polyphonic transcription, downbeat tracking on off-grid material, instrument identification. Classic DSP does all of these and does them WORSE than ML; choosing DSP buys reproducibility, not correctness. Same wrong answer every time is still wrong.

Key correction the interview produced: 'deterministic' and 'not a guess' are different axes. Conflating them was the trap.

## Tier 3 is admitted, with structural provenance

DECIDED (user, against the interviewer's recommendation to ban it): tier 3 ships, labelled.

Every estimated value arrives as an OBJECT, never a bare scalar:

  {"value": "F minor", "source": "estimated", "method": "chroma/Krumhansl-Schmuckler", "confidence": 0.71}

Project-read and measured values stay bare scalars. So a bare value is by construction a fact, and anything wearing a wrapper is a guess. Prose disclaimers in tool descriptions were rejected — they do not survive the model skimming, and a wrapper is mechanically checkable in tests.

PROVENANCE REACHES THE PRODUCER, not only the model: a Suggestion or comment that depended on an estimate is marked as such in the conversation panel, with the estimate and its confidence inspectable. Low confidence also changes the Collaborator's behaviour — it hedges in the reply, it does not silently proceed. This is a constraint on u64tso's settled UX, recorded here because it was discovered here.

## The tool set — seven read-only tools

1. list_tracks — project skeleton: id, name, instrument, role, routing, group membership, mixer state (volume, pan, mute, solo, SENDS), and what data exists per track (clip count, has-MIDI, plugin names, automated parameters).
2. get_arrangement — key, tempo, time signature, bar count, section list, and every track's clip placements.
3. get_midi — MIDI patterns as raw note lists (beat offset, length, pitch, velocity).
4. get_track_analysis — tier 2 measurement of one track's rendered output, WITH AN OPTIONAL BAR RANGE defaulting to the whole track. Per-clip analysis was rejected: the bar range subsumes it and clips do not align across tracks.
5. get_plugin_chain — the chain in order with parameter values (see below).
6. get_automation — NEW, split out of get_plugin_chain. Automation is an independent edit domain and applies to fader and pan, not only plugin parameters.
7. estimate_audio_content — the tier-3 layer, deliberately named so the tool's own name carries its provenance and the model cannot call it without knowing what it is getting.

Buses are tracks — no separate tool. The master and group buses are read through the same five track tools.

SELECTION AND TRANSPORT ARE NOT TOOLS. hll1mo settled that inline entry points pass the producer's selection as implicit context; that plus the playhead goes into the run's OPENING CONTEXT. They describe the producer, not the project, and they are true at run start and nowhere else.

NO PRE-SEEDING. The model starts blind and calls list_tracks/get_arrangement itself, as it did in all 14 prototype runs. Pre-seeded state is a second copy of the truth that can go stale mid-run, it silently caps project size before the prompt blows out, and it breaks the single rule that everything the model knows it asked for and the trace shows it.

NO DIAGNOSTIC TOOLS. Tools measure, the model reasons. No find_masking, no check_gain_staging, no detect_duplicate_parts. Every diagnostic tool is a musical opinion hard-coded in C++ that COMPETES with the model rather than feeding it — a false positive teaches the model to trust a bad verdict, a false negative suppresses a real finding. fod077 is direct evidence they are unnecessary: the models found the masking, diffed the two leads and spotted the +12 duplicate, and compared crest factors across the bus, all unaided.

## Plugin parameters — the gap the fixtures hid

The fod077 fixtures showed plugin chains as 'EQ Eight' with parameters like lowShelfGainDb: -3.0. Real hosted VST3/CLAP plugins expose an opaque indexed parameter list, frequently normalised 0-1, with vendor-chosen names and no machine-readable units. Both prototype models proposed exact parameter values, and that worked only because the fixture was hand-written.

DECIDED: get_plugin_chain returns, per parameter, the vendor name, the normalised value, and THE PLUGIN'S OWN DISPLAY STRING — what its UI would show ('4.00:1', '-18.0 dB') — so the model reads the same text the producer reads. No unit inference, no Duet-side parameter dictionary, no per-plugin mapping table.

Built-in and external plugins share ONE tool and one shape, and the tier does the work: Duet's own EQ/compressor/reverb return bare scalars with real units (tier 1, Duet defines the names), external plugins return the wrapper (tier 3, semantics pattern-matched from a display string). The Collaborator may both comment on and suggest changes to external plugin parameters; such a Suggestion inherits the estimate-marking above.

## Deterministic vs ML — decided per capability, not as a blanket

Performance is NOT the deciding axis: these tools run offline on demand with a few seconds acceptable (settled below), and both Basic Pitch and a chroma key detector are far inside that budget. The binding axis is DEPENDENCY WEIGHT.

- Key and chords -> DSP. Chroma + Krumhansl-Schmuckler is short, textbook, zero dependency, and the accuracy gap on key specifically is small.
- Monophonic pitch -> DSP (YIN/autocorrelation). Arguably tier 2 rather than tier 3: periodicity is a measurable signal property, not a musical guess.
- Onsets -> DSP (spectral flux).
- Polyphonic transcription -> ML. DSP is not merely worse here but not viable. DECIDED: it SHIPS in milestone one, accepting the ONNX Runtime + RTNeural dependency; the user will drop it if the runtime causes build trouble.
- Tempo and downbeats -> NEITHER. Tier 1 covers it; the project owns the tempo map. Beat This solves a problem Duet only has for off-grid imported audio.

LICENCE FINDING THAT INVERTS THE OBVIOUS INTUITION: there is no permissively-licensed C++ MIR library. aubio is GPL-3 (README, primary source), libKeyFinder is GPL-3, Gist is GPL-3, Essentia is AGPL-3 — the entire classic-DSP ecosystem is copyleft and all four are ruled out by the posture that already killed Carla and madmom. Meanwhile Beat This is MIT with MIT weights and Basic Pitch is Apache-2.0. THE LICENCE-CLEAN OPTIONS ARE THE ML ONES. Licensing is therefore not a reason to prefer DSP; the DSP algorithms must be hand-written either way.

TRANSCRIPTION PATH IS PROVEN IN OUR EXACT FRAMEWORK: NeuralNote (github.com/DamRsn/NeuralNote, Apache-2.0) is a JUCE plugin that runs Basic Pitch in C++ — it splits the CNN into four sequential models for RTNeural (BSD-3) and uses ONNX Runtime (MIT) for the constant-Q + harmonic-stacking front end. Readable reference implementation, clean licences throughout. The ONNX per-platform binary is a build-matrix problem for psmj4y, not a runtime one.

## Where measurement runs

lf8tnt found Tracktion's level measurement is a runtime meter with no offline per-track loudness API, so tier 2 means rendering each track offline and analysing the result.

DECIDED: analysis is computed ON DEMAND when the tool is called, CACHED per track keyed on that track's edit state, and INVALIDATED by any edit that changes its output. A repeat call is free, a stale call recomputes, the model never reasons about caching. Analysing the whole project up front was rejected — it spends real time on tracks the model never asks about, and the prototype showed it asks about two or three.

A multi-second pause on the first analysis call is ACCEPTED: runs are non-blocking (hll1mo), and the user's framing is that AI collaboration is an async workflow — request work, come back when it is done.

## Terms recorded

docs/GLOSSARY.md gains 'Tool Vocabulary' and 'Provenance'. The 'Collaborator' entry was corrected in the same edit: it said the Collaborator 'consumes audio on demand', which the AI data strategy retired and this node contradicts outright.

## Why

The provenance principle is what keeps the AI seam honest without pretending the model is looking at a screen: it licenses measurements no UI shows, while making a guess structurally impossible to mistake for a fact. Admitting tier 3 with labelling rather than banning it buys real capability — harmony feedback on recorded audio — at the cost of machinery that turned out to be needed anyway for external plugin parameters, which are the same problem wearing different clothes. Tools measure and the model reasons because fod077 proved the reasoning does not need help and a hard-coded opinion cannot be argued with. The tool set stays small and closed because the prototype showed the model interrogates rather than demands, and every tool added is a surface that must stay true.

## Follow-up carried forward

fod077's fixtures become regression material for this vocabulary and still carry its recorded bugs (fixture-a's inconsistent C1/C2 pitch naming; fixture-f's pad/Rhodes rub). They now also predate the provenance wrapper, get_automation, and the display-string plugin shape, so they need updating to the settled schemas before they serve as regression fixtures.
