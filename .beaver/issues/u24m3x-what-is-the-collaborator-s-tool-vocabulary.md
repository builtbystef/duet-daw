---
id: u24m3x
title: What is the Collaborator's tool vocabulary?
state: todo
priority: high
labels:
    - roadmap:d9gioe
    - session:grill
parent: d9gioe
created: 2026-08-08T03:35:57Z
updated: 2026-08-08T03:35:57Z
---

Grill session. Node lxwoas chose the full Collaborator shape for milestone one, so the perception-side vocabulary must cover every domain the closure principle covers. Settle, as the mirror of hll1mo's edit vocabulary:

- Which deterministic analyses the model can call, and what each returns (the tool contracts / schemas).
- The closure principle that says the set is sufficient — the perception-side analogue of "a Proposal may contain exactly what the producer could do through the UI". Candidate phrasing to test in the interview: the model may read anything the producer can see or hear-as-a-measurement in the UI.
- Which analyses are computed versus read straight off the engine's Edit ValueTree.

Inputs to read first:

- fod077's validated five-tool draft (list_tracks, get_arrangement, get_track_analysis, get_midi, get_plugin_chain) and its fixture schemas — explicitly named the first draft of this vocabulary.
- lf8tnt's finding: Tracktion's level measurement is a runtime meter; no offline deterministic per-track loudness API was found, so measured loudness likely must be built.
- sdfjqh's analysis-library licence findings for the deterministic layer: Beat This (MIT code and weights) and Basic Pitch (Apache-2.0) are clean; madmom's models are CC-BY-NC-SA-4.0 and Essentia is AGPL-3.0 with CC-BY-NC-ND models — both landmines under the licensing posture.

Feasibility facts (what a library can measure, under which licence) are found in-session; the decisions are the user's. Deliverable: the tool list with contracts and the sufficiency principle, ready for the AI-area spec (o3mgk1).
