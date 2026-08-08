---
id: d9gioe
title: Duet DAW — roadmap
state: in-progress
assignee: builtbystef
priority: high
labels:
    - roadmap
created: 2026-08-07T06:01:34Z
updated: 2026-08-08T03:36:44Z
---

## Goal

Duet DAW is a native C++ desktop application that a producer can genuinely make music in — record, arrange, edit, and mix audio and MIDI — with an AI collaborator built into the session as a first-class participant, not a bolted-on chatbot. The first milestone is a complete DAW core (the contents are in node kimula's closing note) plus one genuinely useful AI interaction, growing from there toward a professional-grade instrument. There is no strict timeline; completeness was chosen over an early cut-down release.

**Licensing posture (2026-08-07):** the project starts as open source, with the potential to become a commercial product later. Every choice of technology, library, and service must keep that path open — prefer permissive or dual-licensable dependencies; a copyleft-only dependency with no commercial-license option forecloses a commercial edition and needs explicit justification.

**AI data strategy (2026-08-07):** the Collaborator perceives the project through deterministic analysis tools the DAW owns, never through audio. The model is given structured text — key, tempo, per-track loudness, spectral balance, MIDI as note lists, instrument identity, arrangement structure — computed by native code and delivered as tool results. The model does not listen to a rendered mix and does not infer musical facts a DSP routine can measure. The reasoning: a tool that measures is right every time and is cheap to test, where a model asked to hear is unverifiable and unevidenced — no vendor claims musical reasoning over audio (node sdfjqh). This makes the **project data model the entire AI surface**: every tool reads from it, so its scriptability is an AI requirement, not only an engine one. It also makes the tool vocabulary a first-class design artifact alongside the edit vocabulary settled at node hll1mo (now node u24m3x).

**Stack settled so far:** JUCE 9 as the application foundation (node 1hn16k) and Tracktion Engine as the engine layer (node lf8tnt) — both dual-licensed, so the open-source build is free and a commercial edition is a purchase-and-subscription decision rather than a rewrite. The engine's `Edit` is a `juce::ValueTree`, which is what makes the AI data strategy above practical: the project data model is traversable, observable, undoable and serialisable out of the box. Duet's own edit vocabulary sits as a layer in front of the engine (node skb4tp), which is also what keeps a future engine swap from being a rewrite. The agent loop is **pi (MIT), run as a sidecar process bundled into the DAW install** (node lxwoas): users authenticate model-provider subscriptions through pi or enter an API key, and tool calls reach the DAW through a thin TypeScript extension shim over a local socket (the seam node hcxgfv documented). The tool contracts transfer unchanged to a native C++ loop, so the sidecar is swappable if it disappoints. Milestone one ships the full Collaborator shape — commentary plus Proposals in every edit domain, with the complete Duet Loop mechanics — with fail-fast offline behavior (node lxwoas).

## Frontier

<!-- In-scope questions that you can see, but cannot phrase sharply yet. They become nodes as the roadmap advances. -->

- Real-time engine internals beyond what adopting Tracktion Engine settles: the engine owns the audio graph, its rebuild-from-model behaviour, and its thread model, so what remains is Duet's own seam — how the UI and the Collaborator communicate with the engine's threads without blocking them. The sidecar decision (node lxwoas) adds a concrete piece: the local-socket protocol between the pi shim and the DAW, and which thread services it.
- Real-time safety rules for AI features — nothing the AI does may ever block or glitch the audio thread.
- What the Collaborator carries across sessions (project memory, taste, prior feedback). Within-session conversation is settled at node hll1mo; only the cross-session part stays open. Where it is stored is node rquzdc.
- Milestone-two sequencing: when each feature deferred from milestone one lands — comping, punch-in, loop-recording, recorded automation modes (touch/latch/write), LV2/AU hosting, pre-fader sends, external hardware routing, a session/clip-launch grid view.
- Testing strategy for real-time audio code.
- Distribution, updates, and code signing per platform. The sidecar decision (node lxwoas) adds: bundling pi's standalone binary per platform, and its unmeasured disk/startup footprint (first measured in node ddp1qt).
- Windows-port audio backend: whether to ship ASIO at all, and on which terms. The free ASIO SDK route is GPLv3 and would infect the whole application; a closed Windows build needs Steinberg's proprietary agreement, whose text is not retrievable from Steinberg's own site (node 1hn16k, Unresolved). Not milestone-one work — Linux ships first, and JUCE's WASAPI backend covers Windows including exclusive and shared-low-latency modes.
- The commercial edition itself: what it is and how it is sold. The licence and CLA half of this became node a3p83b; the product half stays here. Node lf8tnt raised the price of that half — a closed edition now needs a JUCE licence plus a Tracktion Engine subscription maintained for as long as the binary is distributed.

## Out of scope

<!-- Items excluded on purpose. The list only grows. One line for each item, with the node's ref if it was a node. An item never moves back in. -->

- Sending audio to the LLM — rejected 2026-08-07 under the AI data strategy: no vendor claims musical reasoning over audio (node sdfjqh), and every audio-derived fact worth having is measurable deterministically. A DSP routine that measures beats a model asked to listen.
- Generative music models — rejected 2026-08-07: no MIDI/symbolic generation, no text-to-music, no stem or audio generation. The Collaborator reasons and proposes edits; it does not synthesise material. Node sdfjqh additionally found that most candidate weights are CC-BY-NC and unshippable under the licensing posture regardless.
- Local-model fallback for the Collaborator — rejected 2026-08-08 at node lxwoas: the Target Producer's hardware cannot run a useful local model, and a silent quality drop would be worse than an honest failure. An unreachable backend fails fast with a visible offline state. Local inference could return only as a deliberate, separately designed offline mode, never as a fallback.
- Mobile or web versions — the product is a native desktop app, decided at the goal level.
- Qt as the application foundation — rejected at node 1hn16k: no MIDI at all, no JACK or ASIO audio backend, no plugin hosting, LGPLv3 relink obligations, and an explicit ban on mixing open-source and commercial Qt in one project.
- Dear ImGui as the application foundation — rejected at node 1hn16k on its own README: it targets tools and debug UIs "as opposed to UI for the average end-user", and accessibility is stated as not supported.
- A custom stack (Skia or NanoVG + SDL3 + RtAudio + libremidi) as the application foundation — rejected at node 1hn16k: all permissive and technically viable, but it hands a solo developer every windowing, text, IME, and plugin-hosting problem at once. NanoVG is additionally unmaintained by its own README.
- Carla as the plugin-hosting layer — rejected at node 1hn16k: the only unified CLAP+VST3 hosting abstraction found, but GPLv2+, which the licensing posture rules out.
- Ambient/unprompted AI observation — rejected at node hll1mo: the Collaborator never speaks or acts uninvited; it fails the taste bar (interrupts flow), and anything it would catch is available on demand by asking for feedback. The only unprompted signal is a finished task run.
- Autonomous AI mutation of the project — rejected at node hll1mo: every Collaborator change enters the project only as a Proposal the producer accepts; nothing changes silently.
- Writing the audio engine from scratch on bare JUCE — rejected at node lf8tnt. JUCE 9 supplies device I/O, format readers/writers, disk streaming, real-time primitives and (verified in source, against the common belief) automatic audio plugin delay compensation in `AudioProcessorGraph` — but it ships no mixer semantics at all (no fader, pan, send, bus or meter node exists), no time-stretch, no metronome, no automation-curve engine, no tempo/musical-time playhead, no session document model, no streaming sampler, and no offline render driver. All of that is milestone-one work, none of it is what differentiates this DAW, and Tracktion Engine's ValueTree-based `Edit` is a better fit for the AI seam than a scratch model would likely have become.
- Rubber Band, SoundTouch and Elastique as the time-stretch backend — rejected at node lf8tnt on licensing. Rubber Band is GPLv2 with a paid commercial alternative and is incompatible with AGPLv3 JUCE without going commercial on both; SoundTouch is LGPL-2.1; Elastique requires an external commercial licence. Signalsmith Stretch is MIT, header-only, and already bundled with Tracktion Engine.
