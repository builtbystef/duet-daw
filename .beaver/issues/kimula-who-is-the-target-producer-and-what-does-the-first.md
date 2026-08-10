---
id: kimula
title: Who is the target producer, and what does the first milestone contain?
state: done
assignee: claude
priority: high
labels:
    - roadmap:d9gioe
    - session:grill
parent: d9gioe
created: 2026-08-07T06:01:43Z
updated: 2026-08-10T18:32:39Z
---

The scoping decision that everything else hangs on. One grill session, limited to:

- Who the target producer is (bedroom producer? electronic? songwriter? engineer?), and which of their workflows the DAW must serve first.
- What "a working DAW core" means concretely for milestone one: audio tracks, MIDI tracks, recording, arrangement editing, mixing, built-in effects — which are in, which wait.
- Which desktop platforms milestone one targets (Linux/Windows/macOS), and in what order.

The answers become the requirements that the framework research (stack evaluation) and the AI interaction-model session both build on.

## Notes

**claude** — 2026-08-07T06:25:04Z

Grill session settled (2026-08-07):

TARGET PRODUCER — the bedroom electronic producer: composes in the box with MIDI and loops, records the occasional audio part, works alone. Milestone one serves this persona end to end. Recorded in docs/GLOSSARY.md as 'Target Producer'.

PLATFORMS — Linux first as the development platform, Windows as the first shipped port, macOS last. The foundation must be chosen so the ports are ports, not rewrites.

MILESTONE-ONE FRAMING — milestone one is the full feature list below, not a cut-down core. No strict timeline; the user builds as time allows and explicitly chose completeness over an early small release.

MILESTONE-ONE CONTENTS:
- MIDI: tracks, piano-roll editing (notes, velocity, quantize), live MIDI recording from hardware controllers.
- Instruments: built-in polyphonic subtractive synth + sample-based drum/sampler instrument, AND plugin hosting.
- Plugin hosting: CLAP + VST3. LV2 and AU deferred (AU only matters once macOS ships).
- Audio: clip playback on audio tracks + single-take recording (record-arm, input selection, monitoring, latency compensation). Comping, punch-in, and loop-recording deferred.
- Arrangement: linear timeline with clips — move/copy/trim/loop, snap to grid. No session/clip-launch grid in milestone one.
- Mixing: per-track volume/pan/mute/solo, master bus, group buses (arbitrary count), send buses (post-fader only), and sidechaining (required — constrains the mixer-graph design). Built-in effects: EQ, compressor, reverb. Pre-fader sends and external hardware routing deferred.
- Automation: drawn lanes for track volume/pan and instrument/effect/plugin parameters. Recorded automation (touch/latch/write) deferred.
- Baseline: project save/load, offline export/bounce to WAV, tempo + metronome.

WHY — this persona is the most likely to embrace an AI collaborator (works alone, iterates fast); Linux-first matches the developer's machine for the fastest loop; CLAP+VST3 cover what producers actually own while deferring platform-bound formats; each deferred item removes a large plumbing area without blocking the persona's core composing workflow.

**claude** — 2026-08-08T07:15:38Z

Amendment (2026-08-08, from node u24m3x's session): MILESTONE-ONE PLUGIN HOSTING NARROWS TO VST3. This note's contents said 'Plugin hosting: CLAP + VST3'. CLAP is now deferred to milestone two, at the user's decision, to reach a functional app sooner.

VST3 comes essentially free through JUCE and Tracktion's ExternalPlugin; CLAP must be hand-written from scratch (hvv3nn found no usable hosting library exists at any licence), so the whole deferred cost was CLAP-specific. Nodes 89jlz1 and 00yd6o closed unresolved as a result.

Everything else in the milestone-one contents stands. The built-in EQ, compressor, reverb, synth and sampler are unaffected — and they now carry more weight, since they are the plugins whose parameter semantics Duet owns outright (see u24m3x on plugin-parameter provenance).

**claude** — 2026-08-10T18:32:39Z

Scope deltas from UI grill s11o4w (2026-08-10), settled against the user's UI mockup: piano roll ADDS scale highlighting + Fold view and a note-length control; browser ADDS search + favorites; Settings ADDS global interface scaling. Built-ins STAY 2 instruments + 3 effects — the mockup's third instrument ('Duet Drum Rack') and six-effect list are set dressing, not scope.
