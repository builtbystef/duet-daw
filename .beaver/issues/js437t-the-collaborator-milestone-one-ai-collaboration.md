---
id: js437t
title: The Collaborator — milestone-one AI collaboration
state: todo
priority: high
labels:
    - spec
depends_on:
    - hll1mo
    - sdfjqh
    - lxwoas
    - u64tso
    - u24m3x
created: 2026-08-08T08:19:47Z
updated: 2026-08-08T08:19:47Z
---

# The Collaborator — milestone-one AI collaboration

## Problem Statement

The Target Producer works alone. There is no second pair of ears in the room: no one to sanity-check a mix that has gone numb after four hours, name why the drop feels weak, or suggest the turnaround the loop is missing. Existing AI tools sit outside the session — chatbots that require exporting audio, describing the project in prose, and translating generic advice back into edits by hand. The flow cost erases the value, and the advice is unmoored from the actual project state.

## Solution

The Collaborator is a participant inside the session. The Target Producer asks it anything about the project — vague ("something's off in the drop") or precise ("give me a turnaround into bar 9") — from a docked conversation panel or from a right-click on any clip or track. The Collaborator interrogates the project itself, through read-only tools that return facts with known provenance; it never listens to audio and never receives a guess disguised as a fact. It answers with commentary, a Proposal, or both. A Proposal materializes in place — ghost clips in the timeline, ghost fader positions in the mixer — auditionable in context, A/B-able during playback, and enters the project only when the producer accepts it, whole or element by element. Rejection with a reason produces a revision. The producer's own edits always win; a Proposal whose target changed is marked stale, never auto-merged. Everything runs without blocking playback, editing, or recording.

## User Stories

1. As the Target Producer, I want to ask the Collaborator open questions about my project and get answers grounded in the project's actual state, so that I get a second pair of ears without leaving the session.
2. As the Target Producer, I want the Collaborator's suggested changes delivered as Proposals that alter nothing until I accept them, so that I keep authorship of the track.
3. As the Target Producer, I want proposed clips and mixer values to materialize in place, visibly marked and playable in context, so that I audition a suggestion instead of imagining it.
4. As the Target Producer, I want an A/B toggle on mix-change Proposals during playback, so that I compare current and proposed by ear.
5. As the Target Producer, I want to accept or reject a Proposal whole, or cherry-pick its elements, so that I keep the half of an idea that works.
6. As the Target Producer, I want to reject with a typed reason and get a revised Proposal back, so that the exchange converges instead of restarting.
7. As the Target Producer, I want a Proposal whose target I have since edited to be marked stale — still auditionable, never auto-merged — with a one-click redo against the current state, so that my live edits always win.
8. As the Target Producer, I want to keep playing and editing while a Task Run is in progress, and to cancel it at any time, so that the Collaborator never blocks my flow.
9. As the Target Producer, I want to invoke the Collaborator from a clip or track's context menu with that item as implicit context, so that asking about *this thing here* takes one gesture.
10. As the Target Producer, I want to use my own model-provider subscription or API key and pick the model, so that I control cost, privacy, and quality.
11. As the Target Producer, I want a failed or offline backend to fail fast with a plain error in the conversation while the DAW keeps working fully, so that AI trouble never becomes DAW trouble.
12. As the Target Producer, I want feedback that depended on an estimated fact (a guessed key, an external plugin's inferred parameter meaning) to be visibly marked and inspectable, so that I can tell measured ground from educated guess.
13. As a developer of Duet, I want a development mode that shows the raw tool-call trace of a run, so that I can debug the Collaborator's behavior; end users see friendly status phrases instead.

## Implementation Decisions

### Architecture: three parts, one seam

- **Collaborator service** — a C++ module in the DAW. Owns the socket server, run lifecycle, tool dispatch, the analysis layer and its cache, the Proposal manager, and the estimate ledger.
- **Sidecar** — a minimal Node host program embedding pi's SDK (`createAgentSession`, built-in tools empty, all tools custom), bundled into the DAW install as a standalone binary, invisible to the user. It owns the agent loop, provider auth, model switching, and streaming. (Refined from lxwoas's recorded RPC+shim shape; decided in this session — one protocol on one socket, the embedding shape fod077 proved.)
- **Socket protocol** — newline-delimited JSON-RPC 2.0 over a local socket (Unix domain socket on Linux). The DAW is the server; it spawns the sidecar with the socket path as an argument. Every prompt, streamed event, cancellation, and tool call crosses this one seam. The tool contracts are transport-independent: they transfer unchanged to a native C++ loop if the sidecar ever disappoints (the recorded escape hatch).

The sidecar is spawned lazily on first Collaborator use, killed on DAW exit, and respawned on demand if found dead. Sidecar death during a run fails that run (story 11); nothing queues.

### Socket protocol methods

DAW → sidecar (requests):

```
configure        { model: string, systemPromptParams: object }        → {}
run.start        { runId: string, prompt: string,
                   openingContext: OpeningContext }                   → {}   (completion signaled via run.finished)
run.cancel       { runId: string }                                    → {}
models.list      {}                                                   → { models: [{ id, provider, authenticated }] }
auth.setApiKey   { provider: string, key: string }                    → {}
auth.beginOAuth  { provider: string }                                 → { url: string, instructions: string }
shutdown         {}                                                   → {}
```

Sidecar → DAW:

```
tool.call        { runId, callId, tool: string, args: object }        → { result: object } | JSON-RPC error
run.text         { runId, delta: string }                             (notification: streamed commentary)
run.toolActivity { runId, tool: string, phase: "start"|"end" }        (notification: dev-mode trace; end users see rotating phrases)
run.finished     { runId, status: "completed"|"canceled"|"failed", error?: string }   (notification)
```

```
OpeningContext {
  selection: { kind: "none" | "clips" | "tracks", ids: string[] },
  playhead:  { bar: number, beat: number },
  transportPlaying: boolean
}
```

Selection and transport are opening context, not tools: they describe the producer, are true at run start and nowhere else (u24m3x). The system prompt is Duet's own — Collaborator identity, the provenance rules, the hedging instruction, Proposal guidance — never pi's coding-agent identity.

### Tool Vocabulary contracts

Seven read-only tools (u24m3x), served by the Collaborator service against the live project model. Provenance is structural: project-read and measured values are bare scalars; every estimated value is wrapped —

```
Estimate<T> { value: T, source: "estimated", method: string, confidence: number /* 0..1 */ }
```

A bare value is by construction a fact; anything wrapped is a guess.

```
list_tracks     {}                                  → { tracks: [{ id, name, kind: "midi"|"audio"|"group"|"master",
                                                        instrument?: string, role?: string, output: trackId,
                                                        mixer: { volumeDb, pan, mute, solo,
                                                                 sends: [{ busId, levelDb }] },
                                                        clipCount, hasMidi, pluginNames: string[],
                                                        automatedParameters: string[] }] }
get_arrangement {}                                  → { key?: Estimate<string>, tempoBpm, timeSignature,
                                                        barCount, sections: [{ name, startBar, endBar }],
                                                        placements: [{ trackId, clips: [{ clipId, name,
                                                          startBar, lengthBars, looped }] }] }
get_midi        { trackId, clipId? }                → { clips: [{ clipId, notes: [{ noteId, pitch,
                                                        startBeats, lengthBeats, velocity }] }] }
get_track_analysis { trackId, barRange?: [start, end] }   /* defaults to whole track */
                                                    → { peakDb, truePeakDbtp, rmsDb, lufsIntegrated,
                                                        lufsShortTermMax, crestFactorDb,
                                                        spectralBands: [{ band, energyDb }],
                                                        spectralCentroidHz, spectralFlatness,
                                                        stereoCorrelation, stereoWidth,
                                                        onsetsBeats: number[] }
get_plugin_chain { trackId }                        → { plugins: [{ pluginId, name, format: "builtin"|"vst3",
                                                        enabled, latencySamples,
                                                        parameters: [ BuiltinParam | ExternalParam ] }] }
get_automation  { trackId }                         → { lanes: [{ target: AutomationTarget,
                                                        points: [{ timeBeats, value }] }] }
estimate_audio_content { trackId, barRange?,
                         aspects?: ["key"|"chords"|"notes"|"instrument"] }
                                                    → { key?: Estimate<string>,
                                                        chords?: Estimate<[{ bar, chord }]>,
                                                        notes?: Estimate<[{ pitch, startBeats, lengthBeats, velocity }]>,
                                                        instrument?: Estimate<string> }

BuiltinParam  { paramId, name, value, unit }                          /* Duet-owned semantics: bare scalars */
ExternalParam { paramId, vendorName, normalizedValue,
                displayString: Estimate<string> }                     /* the plugin's own UI text; semantics estimated */

AutomationTarget = { kind: "volume" } | { kind: "pan" }
                 | { kind: "pluginParam", pluginId, paramId }
```

Buses are tracks: master and groups are read through the same tools. Spectral bands are a fixed named set (sub, low, low-mid, mid, high-mid, high, air) whose edges the tool description documents — the "measured by a documented routine" requirement. `get_arrangement.key` is present only when the project declares a key; otherwise the model asks `estimate_audio_content`.

There are no diagnostic tools, no pre-seeded state, no selection/transport tools (all settled at u24m3x). The model starts blind and asks; everything it knows is in the trace.

### The `propose` tool and the edit-operation vocabulary

A Proposal is emitted through one write-tool, callable at most once per Task Run for a new Proposal (revisions replace — below). Commentary is plain assistant text; a run may produce commentary, a Proposal, or both.

```
propose { summary: string,
          elements: [{ description: string, operations: EditOperation[] }] }
        → { proposalId } | validation error (unknown ids, out-of-range values;
                                             the model may correct and retry)
```

An **element** is one human-meaningful change (the row with ✓/✗ on the Proposal card) and may bundle several operations — "sidechain the bass to the kick" is one element carrying add-plugin, set-params, and set-sidechain-source operations. Elements must be independently applicable; cherry-pick granularity is the element.

`EditOperation` is a tagged union mirroring exactly what the Target Producer can do through the milestone-one UI (the closure principle, hll1mo; surface from kimula):

```
/* MIDI (piano-roll) */
midi.addNotes      { clipId, notes: [{ pitch, startBeats, lengthBeats, velocity }] }
midi.removeNotes   { clipId, noteIds: string[] }
midi.modifyNotes   { clipId, changes: [{ noteId, pitch?, startBeats?, lengthBeats?, velocity? }] }

/* Clips on the linear timeline */
clip.createMidi    { trackId, startBar, lengthBars, name?, notes?: [...] }
clip.delete        { clipId }
clip.move          { clipId, trackId?, startBar }
clip.trim          { clipId, startBar, lengthBars }
clip.setLoop       { clipId, looped: boolean, loopLengthBars? }
clip.duplicate     { clipId, toTrackId?, atBar }

/* Tracks and routing */
track.create       { kind: "midi"|"audio"|"group", name, instrument? /* builtin synth|sampler */ }
track.rename       { trackId, name }
track.delete       { trackId }
track.setOutput    { trackId, busId }

/* Mixer */
mixer.set          { trackId, volumeDb?, pan?, mute?, solo? }
mixer.setSend      { trackId, busId, levelDb }

/* Plugins */
plugin.add         { trackId, position, plugin: { builtin: "eq"|"compressor"|"reverb"|"synth"|"sampler" }
                                        | { external: knownPluginId } }
plugin.remove      { pluginId }
plugin.reorder     { trackId, pluginId, position }
plugin.setParam    { pluginId, paramId, value }      /* builtin: real units; external: normalized 0..1 */
plugin.setSidechainSource { pluginId, sourceTrackId }

/* Automation (drawn lanes) */
automation.setPoints    { trackId, target: AutomationTarget, points: [{ timeBeats, value }] }
automation.removePoints { trackId, target: AutomationTarget, range: [startBeats, endBeats] }

/* Project */
project.setTempo         { bpm }
project.setTimeSignature { numerator, denominator }
```

No operation creates audio content: the Collaborator can move, trim, loop, duplicate, and delete existing audio clips but cannot introduce new audio (no generation — Out of scope on the roadmap root). Quantize and similar UI conveniences are expressible as `midi.modifyNotes`.

### Proposal manager

States: **pending → accepted | rejected | superseded**, orthogonal flag **stale**.

- Multiple pending Proposals coexist independently.
- **Stale**: any producer edit that touches an entity referenced by a pending Proposal's operations flips that Proposal (and its ghost marks) stale — still auditionable, never auto-merged. "↻ Redo against current state" resolves the stale Proposal as superseded and starts a fresh run with the original request plus the intervening reality.
- **Cherry-pick**: accepting an element materializes exactly that element's operations; rejecting an element removes its ghost marks. A card resolves when every element is resolved. "Accept all" / "Reject" are the whole-Proposal fast paths.
- **Revision**: replying to a pending Proposal supersedes it with the revised one; replying to a rejected Proposal yields a new pending Proposal. Rejection-with-a-reason is first-class input to the revision run.
- **Undo requirement**: accepting a Proposal (or an element) lands in the shared undo history as one undoable action. The mechanism belongs to the shared-undo design (open node skb4tp); this spec only binds the observable behavior.
- Proposals and the conversation are in-memory per app session; both die with the app. Persistence across restarts arrives with the project-data decision (open node rquzdc), not here.

Audition mechanics — how ghost clips and ghost values technically enter the playback graph without entering the project state — are bound here only behaviorally (in-place, playable in context, A/B for mix changes, producer edits win); the mechanism is foundation-area work (86t5lu, skb4tp).

### Estimate marking

Mechanical taint, never model self-report: the Collaborator service keeps a per-run **estimate ledger** of every wrapped value returned to the model (`estimate_audio_content` results, `ExternalParam.displayString`). Once the ledger is non-empty, every subsequent `propose` and all subsequent commentary in that run is marked "based on estimates," with the ledger inspectable from the panel. The system prompt additionally instructs hedging when confidence is low, but the marking never depends on the model's cooperation. Over-marking (a run that glanced at an estimate, then proposed something unrelated) is accepted; narrowing it later is a presentation change, not a contract change.

### Task Run lifecycle

One active run at a time; the panel's input box is disabled while a run is in progress. A run is non-blocking for the DAW — playing, editing, and recording continue. Cancel tears down client-side (`run.cancel` → sidecar `abort()`), leaves a "task canceled, nothing changed" line, and re-enables input. Backend unreachable, provider error, or sidecar death: the run fails fast with a plain transient error line in the conversation ("The Collaborator isn't working right now — try again later"); no dedicated offline UI state, nothing queues, no retry loop, the DAW is otherwise fully functional.

### Analysis layer

- **Tier 1** (read): everything in the project model — exact, no analysis.
- **Tier 2** (measured, hand-written DSP; no third-party MIR dependency exists at an acceptable licence): RMS, peak, true peak, integrated and short-term LUFS per ITU-R BS.1770, crest factor, spectral band energies, centroid, flatness, stereo correlation and width, onsets via spectral flux, monophonic f0 via YIN.
- **Tier 3** (estimated, wrapped): key and chords via chroma + Krumhansl-Schmuckler (DSP); polyphonic transcription via Basic Pitch on ONNX Runtime + RTNeural (the one ML dependency, NeuralNote-proven, droppable if the runtime causes build trouble); instrument identity.

Analysis input is the track's rendered output, produced through the engine's offline render path. Computation is on demand at tool-call time, cached per track keyed on that track's edit state, invalidated by any edit that changes its output. A multi-second first call is accepted; runs are non-blocking.

### Real-time safety rules

Nothing the Collaborator does may ever block or glitch the audio thread — by construction, not by care:

- The socket is serviced by a dedicated Collaborator-service thread.
- Project-model reads for tool results execute on the message thread (the service thread marshals and waits); they are reads of the authoritative state, never of a second copy.
- Tier-2/3 analysis and offline renders run on worker threads.
- No AI-related code shares a lock with the audio callback. Proposal audition plays through the engine's ordinary playback mechanisms.

### Model access

Bring-your-own credentials through pi's provider layer: subscription OAuth or API keys, surfaced in a setup UI backed by the `auth.*` and `models.list` methods. A model picker exposes whatever the user configured. Recommended default: `gpt-5.6-terra` (7/7 in fod077) when available, otherwise the first configured provider. No provider is treated specially in the picker. With no credentials configured the panel shows a setup state, not an error.

### Conversation panel and surfaces

As settled at u64tso, normative here: docked right; producer messages carry a selection context chip; Collaborator commentary in accent bubbles; quick-suggestion chips adapt to selection; a Task Run is a card with spinner, "you can keep editing" hint, Cancel, and rotating friendly status phrases. Raw tool calls are development-mode only. Inline entry is "✦ Ask Collaborator" in the ordinary right-click context menu of a clip or track (no floating affordances; clicking a clip only selects it); it sets the implicit context and focuses the composer. Timeline proposal-state: ~60% opacity + glowing accent ring + ✦ badge, auditionable, not draggable. Mixer proposal-state: ghost fader handle at the proposed value; ✦-marked parameter lines under the strip; A/B toggle (A current / B proposed) swaps heard values during playback.

## Dependencies

- **pi** (`@earendil-works/pi` packages, MIT) — the agent loop: multi-provider auth and abstraction, session management, streaming, `abort()`. Run out of process; never linked against JUCE.
- **A Node-to-standalone-binary bundler** for the sidecar (pi's own released binaries prove the approach; the exact tool is chosen when ddp1qt measures the footprint). The sidecar binary ships inside the DAW install.
- **ONNX Runtime** (MIT) and **RTNeural** (BSD-3) — polyphonic transcription via Basic Pitch, NeuralNote-style. Explicitly separable: dropped if the runtime causes build trouble. Basic Pitch model weights: Apache-2.0 per repository, but verify the weights' licence specifically before shipping (open question recorded at sdfjqh).
- **A JSON library for the DAW side** (nlohmann/json, MIT) — JSON-RPC framing and tool payloads.
- **No HTTP client in the DAW**: all provider HTTP lives in the sidecar. The libcurl recommendation from sdfjqh applies only if the native-loop escape hatch is ever exercised.
- Local sockets use JUCE's own primitives — no new dependency.

## Testing Decisions

Three seams, outermost first (agreed in this session):

1. **The socket protocol** — the primary seam. A test harness plays sidecar against the real Collaborator service: drives every tool against a fixture project and asserts the contracts above; submits `propose` calls and asserts validation (unknown ids, out-of-range values rejected with correctable errors); asserts the estimate ledger's taint behavior (a run that received a wrapped value → subsequent Proposal marked); asserts run lifecycle (cancel, failure, single-active-run). No LLM, no Node, no UI. This seam doubles as the escape-hatch guarantee: anything that speaks the protocol can replace the sidecar.
2. **The deterministic analysis routines as pure functions** — waveform in, measurements out. Worked examples:
   - A full-scale 1 kHz sine: peak 0 dBFS, RMS −3.01 dB, crest factor 3.01 dB; a square wave: crest 0 dB.
   - ITU-R BS.1770 / EBU R128 conformance: the EBU Tech 3341 test signals with their published expected LUFS values (e.g. the −23.0 LUFS reference cases within ±0.1 LU).
   - YIN: a 440 Hz sine → f0 440 Hz within ±1 cent; silence → no pitch.
   - Onsets: a rendered click pattern at known beat positions → onset times within a few milliseconds.
   - Key estimation: a rendered C-major triad progression → an `Estimate` wrapper (never a bare value) with value "C major" and plausible confidence.
3. **The Proposal manager's C++ interface** — the state machine: pending/accepted/rejected/superseded transitions, stale flagging on a targeting edit, cherry-pick leaving the remainder pending, revision superseding, accept-as-one-undoable-action (behavioral assertion against the undo seam once skb4tp lands).

Good tests here assert external behavior only: protocol responses, measured values, state transitions — never internal call sequences. The fod077 fixtures, updated to these schemas (their recorded bugs fixed: consistent pitch naming, the fixture-f pad/Rhodes rub made deliberate or removed), become regression fixtures served through the protocol seam. Prior art: none — this is the project's first code area.

## Out of Scope

- Sending audio to the model; generative music models (roadmap root).
- Ambient/unprompted observation; autonomous mutation (hll1mo).
- Local-model fallback (lxwoas); any dedicated persistent offline UI state (u64tso).
- Diagnostic tools; pre-seeded project state; selection/transport tools (u24m3x).
- Concurrent Task Runs (this session): one run at a time in milestone one.
- Conversation/Proposal persistence across app restarts (open node rquzdc) and cross-session memory (Frontier).
- The shared undo mechanism (open node skb4tp) and the technical audition mechanism (foundation area, 86t5lu) — this spec binds their observable behavior only.
- Raw tool-call traces for end users (u64tso — development mode only).
- CLAP hosting (deferred to milestone two; `get_plugin_chain.format` gains a value then).

## Further Notes

- **Escape hatch**: the sidecar is swappable by design. The tool and protocol contracts are the stable artifact; pi is an implementation detail behind them.
- **Sidecar footprint** (disk, startup latency) is unmeasured — ddp1qt measures it. If it disappoints badly enough, the native loop replaces the sidecar without contract changes.
- **Prompt-cache discipline** (sdfjqh): tool results and system prompt should keep frozen content first and volatile deltas last; no timestamps or unordered serialization in stable prefixes.
- **ADRs recorded with this spec**: (1) the Collaborator perceives through deterministic tools, never audio; (2) the agent loop is a bundled pi-SDK sidecar behind a socket protocol. ARCHITECTURE.md gains the AI seam.
- **Constraint inherited by the panel UX** (u24m3x): estimate-marked output is inspectable and the Collaborator hedges at low confidence.
- The recommended-default model claim (`gpt-5.6-terra`) rests on a 14-run prototype; it is a default, not a benchmark verdict. Revisit when a wider comparison is worth running.
