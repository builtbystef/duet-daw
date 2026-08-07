---
id: d9gioe
title: Duet DAW — roadmap
state: in-progress
assignee: builtbystef
priority: high
labels:
    - roadmap
created: 2026-08-07T06:01:34Z
updated: 2026-08-07T21:38:52Z
---

## Goal

Duet DAW is a native C++ desktop application that a producer can genuinely make music in — record, arrange, edit, and mix audio and MIDI — with an AI collaborator built into the session as a first-class participant, not a bolted-on chatbot. The first milestone is a complete DAW core (the contents are in node kimula's closing note) plus one genuinely useful AI interaction, growing from there toward a professional-grade instrument. There is no strict timeline; completeness was chosen over an early cut-down release.

**Licensing posture (2026-08-07):** the project starts as open source, with the potential to become a commercial product later. Every choice of technology, library, and service must keep that path open — prefer permissive or dual-licensable dependencies; a copyleft-only dependency with no commercial-license option forecloses a commercial edition and needs explicit justification.

**AI data strategy (2026-08-07):** the Collaborator perceives the project through deterministic analysis tools the DAW owns, never through audio. The model is given structured text — key, tempo, per-track loudness, spectral balance, MIDI as note lists, instrument identity, arrangement structure — computed by native code and delivered as tool results. The model does not listen to a rendered mix and does not infer musical facts a DSP routine can measure. The reasoning: a tool that measures is right every time and is cheap to test, where a model asked to hear is unverifiable and unevidenced — no vendor claims musical reasoning over audio (node sdfjqh). This makes the **project data model the entire AI surface**: every tool reads from it, so its scriptability is an AI requirement, not only an engine one (bears on node lf8tnt). It also makes the tool vocabulary a first-class design artifact alongside the edit vocabulary settled at node hll1mo.

## Frontier

<!-- In-scope questions that are too vague to be nodes. They become nodes as the roadmap advances. -->

- The Collaborator's tool vocabulary: which deterministic analyses the model can call, what each returns, and the closure principle that says the set is sufficient. The mirror of hll1mo's edit vocabulary, on the perception side.
- Whether the agent loop is native C++ or a bundled sidecar harness (pi is MIT and the strongest candidate; the cost is a second runtime inside an audio app). Not urgent — the tool contracts transfer either way.
- Project persistence: what the session/project file format is, and how it versions.
- Undo/redo model for a session that both the human and the AI edit — node hll1mo settled that one edit vocabulary is shared by both, which is the constraint this design starts from.
- Real-time engine internals: thread model, lock-free UI↔audio communication, the project data model in memory.
- Real-time safety rules for AI features — nothing the AI does may ever block or glitch the audio thread.
- What the Collaborator carries across sessions (project memory, taste, prior feedback). Within-session conversation is settled at node hll1mo; only the cross-session part stays open.
- Milestone-two sequencing: when each feature deferred from milestone one lands — comping, punch-in, loop-recording, recorded automation modes (touch/latch/write), LV2/AU hosting, pre-fader sends, external hardware routing, a session/clip-launch grid view.
- Testing strategy for real-time audio code.
- Distribution, updates, and code signing per platform.
- Windows-port audio backend: whether to ship ASIO at all, and on which terms. The free ASIO SDK route is GPLv3 and would infect the whole application; a closed Windows build needs Steinberg's proprietary agreement, whose text is not retrievable from Steinberg's own site (node 1hn16k, Unresolved). Not milestone-one work — Linux ships first, and JUCE's WASAPI backend covers Windows including exclusive and shared-low-latency modes.
- The commercial edition itself: what it is and how it is sold. The licence and CLA half of this became node a3p83b; the product half stays here.

## Out of scope

<!-- Items excluded on purpose. The list only grows. One line for each item, with the node's ref when it was one. An item never moves back in. -->

- Sending audio to the LLM — rejected 2026-08-07 under the AI data strategy: no vendor claims musical reasoning over audio (node sdfjqh), and every audio-derived fact worth having is measurable deterministically. A DSP routine that measures beats a model asked to listen.
- Generative music models — rejected 2026-08-07: no MIDI/symbolic generation, no text-to-music, no stem or audio generation. The Collaborator reasons and proposes edits; it does not synthesise material. Node sdfjqh additionally found that most candidate weights are CC-BY-NC and unshippable under the licensing posture regardless.
- Mobile or web versions — the product is a native desktop app, decided at the goal level.
- Qt as the application foundation — rejected at node 1hn16k: no MIDI at all, no JACK or ASIO audio backend, no plugin hosting, LGPLv3 relink obligations, and an explicit ban on mixing open-source and commercial Qt in one project.
- Dear ImGui as the application foundation — rejected at node 1hn16k on its own README: it targets tools and debug UIs "as opposed to UI for the average end-user", and accessibility is stated as not supported.
- A custom stack (Skia or NanoVG + SDL3 + RtAudio + libremidi) as the application foundation — rejected at node 1hn16k: all permissive and technically viable, but it hands a solo developer every windowing, text, IME, and plugin-hosting problem at once. NanoVG is additionally unmaintained by its own README.
- Carla as the plugin-hosting layer — rejected at node 1hn16k: the only unified CLAP+VST3 hosting abstraction found, but GPLv2+, which the licensing posture rules out.
- Ambient/unprompted AI observation — rejected at node hll1mo: the Collaborator never speaks or acts uninvited; it fails the taste bar (interrupts flow), and anything it would catch is available on demand by asking for feedback. The only unprompted signal is a finished task run.
- Autonomous AI mutation of the project — rejected at node hll1mo: every Collaborator change enters the project only as a Proposal the producer accepts; nothing changes silently.
