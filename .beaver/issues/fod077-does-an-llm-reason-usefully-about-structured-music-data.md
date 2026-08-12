---
id: fod077
title: Does an LLM reason usefully about structured music data with no audio?
state: done
assignee: claude
priority: high
labels:
    - roadmap:d9gioe
    - session:prototype
parent: d9gioe
created: 2026-08-07T21:40:15Z
updated: 2026-08-07T23:04:45Z
---

Prototype session. Answer one question before any DAW code exists:

**Given structured, tool-produced music data and no audio, does an LLM say anything a producer would actually act on?**

The whole AI direction rests on this. The roadmap's AI data strategy (node d9gioe) commits to the Collaborator perceiving the project only through deterministic analysis tools. Node sdfjqh established that no vendor claims musical reasoning over audio; this node tests whether the symbolic substitute reasons any better. It needs no engine, no toolchain, no C++ — only fixtures, a harness, and accounts already held.

## Harness: a small Electron app with pi embedded

Build a throwaway Electron app that embeds pi in-process, the way OpenClaw does (node hcxgfv): `createAgentSession()` imported directly, `builtInTools` left empty, everything supplied through `customTools`. Electron is Node, so the embedding is free here — no sidecar, no IPC, no bundled second runtime.

**Serve the fixtures through tools, not in the prompt.** This is the point of using a real harness rather than curl. Do not paste a wall of JSON into the system prompt; register tools — `list_tracks`, `get_track_analysis`, `get_midi`, `get_arrangement`, `get_plugin_chain` — that read from the fixture and return it on demand. Then the experiment tests the architecture actually being proposed: whether the model *asks the right questions* and assembles a picture from tool calls, not merely whether it can summarise a document. A model that calls three tools and finds the problem is a very different result from one that needs everything up front, and only this shape distinguishes them.

The app needs no polish: a fixture picker, a prompt box, the streamed response, and a visible log of which tools the model called in what order. That tool-call trace is half the finding.

**What this does and does not prove.** It validates the tool vocabulary, the data shapes, and the interaction — all of which transfer. It does **not** answer whether pi can be embedded in DuetDAW: Electron is the environment where embedding is trivially free, and the C++/JUCE question (bundled runtime, no host tools over RPC) stays open at node hcxgfv. Do not let a successful prototype be read as a decision to ship pi.

## Models

Run every fixture through both. pi makes this a keystroke: authenticate each with `/login` or an API key, then `--models` with a comma-separated list and Ctrl+P / Shift+Ctrl+P to cycle, or Ctrl+L for the model selector.

- **GPT-5.6 Terra** (OpenAI) — usable through the existing Codex subscription; pi supports ChatGPT Plus/Pro subscription auth directly, so no separate API billing.
- **Grok 4.5** (xAI) — xAI is a built-in pi provider.

Both are already accessible, so the run costs nothing extra. Select with `--model provider/id`; pi accepts `provider/id` patterns with optional `:thinking` levels, so reasoning effort is also cheap to vary.

Other vendors are deliberately out of this run. This node tests whether the *approach* works, not which model wins — two models are enough to tell a direction failure from a model failure, and a broader comparison is only worth running once the approach has survived. Claude in particular is a live candidate whenever that happens: node sdfjqh excluded it solely for lacking documented audio input, an objection the AI data strategy has since made moot.

## Method

1. **Write 6–8 fixtures by hand** as JSON, each describing one musical situation recognisable from real sessions. Draw them from actual projects, not invented ones — the point is to judge output as the person who already knows what the right answer was. Suggested spread:
   - a mix where the kick and bass are masking each other
   - an eight-bar loop that has gone boring and needs an arrangement move
   - a chord progression that wants a turnaround
   - a track that is over-compressed and needs its dynamics back
   - a busy arrangement where something should be cut
   - a situation with *nothing wrong* — a control, to see whether the model invents problems

2. **Encode only what a deterministic tool could really produce.** Key, tempo, time signature, bar count, arrangement sections, and per track: instrument identity, role, RMS/peak/LUFS, spectral balance in bands, MIDI as note lists (pitch, start, duration, velocity), plugin chain with parameter values. If a field could not be computed by native DSP or read from the project model, it does not belong in the fixture — including it fakes the experiment.

3. **Ask for both** a critique and a Suggestion-shaped set of concrete edits, so the output is testable against node hll1mo's Suggestion vocabulary as well as for musical sense.

4. **Judge as a producer, not as an engineer.** Sort each response into: *acted on it*, *true but useless*, *generic advice any forum post would give*, *wrong*. Generic advice is the failure mode to watch for — "try sidechaining the bass to the kick" is what a model says when it has understood nothing. The control fixture catches the other failure mode: inventing problems to look useful.

## Deliverable

A note recording, per fixture and per model, what came back, which tools it called in what order, and which bucket the response fell in — plus a verdict on whether the direction survives. If it does, the fixture schemas and the tool set are the first draft of the Collaborator's tool vocabulary and feed straight into node lxwoas. If it does not, lxwoas must be reopened at the level of the interaction model, not the interaction.

Keep the fixtures and the tool schemas in the repository; they become regression material for the real tool layer later.

## Notes

**claude** — 2026-08-07T21:49:01Z

Scope set by the user (2026-08-07): harness is a throwaway Electron app with pi embedded in-process (createAgentSession, builtInTools empty, fixtures served through customTools). Models: GPT-5.6 Terra via the existing OpenAI Codex subscription (pi supports ChatGPT Plus/Pro subscription auth), Grok 4.5 via pi's built-in xAI provider, and Claude as a third — sdfjqh ruled Claude out only for lacking audio input, which is now moot. Both OpenAI and xAI access are already held, so the run costs nothing extra.

**claude** — 2026-08-07T21:52:08Z

Correction to the scope note above (2026-08-07): Claude is dropped from this run at the user's direction. The run is GPT-5.6 Terra and Grok 4.5 only. Two models are enough to separate a direction failure from a model failure; a wider vendor comparison is deferred until the approach itself has survived.

**claude** — 2026-08-07T22:17:42Z

Harness built (2026-08-07): prototype-llm-music-reasoning/ — Electron app with pi 0.84.1 embedded in-process (createAgentSession, noTools:'builtin', sealed DefaultResourceLoader so no local skills/extensions/AGENTS.md leak). Five customTools serve the fixture: list_tracks, get_arrangement, get_track_analysis, get_midi, get_plugin_chain. Seven draft fixtures (a: kick/bass masking, b: static loop, c: missing turnaround, d: over-compressed drum bus, e: busy drop with a literal +12 duplicate lead, f: clean control, g: buried hook) plus fixtures/GROUND_TRUTH.md (never served). Both models resolve via existing OAuth (openai-codex/gpt-5.6-terra, xai/grok-4.5); token-free smoke test passes; app launches (--no-sandbox needed on this machine). Every run auto-saves to runs/ with full tool trace and messages. Awaiting: user correction of fixtures/ground truth against real sessions, then model runs and per-fixture bucket judgments.

**claude** — 2026-08-07T23:04:29Z

Run review (2026-08-07). 14 runs in prototype-llm-music-reasoning/runs/: 7 fixtures x 2 models (openai-codex/gpt-5.6-terra thinking:medium, xai/grok-4.5), each from the fixture's vague suggestedPrompt, fixtures served only through the five customTools. Verdicts are judged against fixtures/GROUND_TRUTH.md, which is still the uncorrected draft — see caveats.

## Per-fixture buckets

**fixture-a (kick/bass masking)** — Terra: ACTED ON IT. Found the 20–150 Hz stack, no-sidechain, sustained bass under every kick; proposed sidechain comp (4:1, −24 dB, 2 ms/110 ms) + HPF + drive and fader trims — matches accepted fix 1. Grok: ACTED ON IT, with the bonus: it noticed kick and bass literally share MIDI pitch 36 (verified in the fixture: kick "C1"=36, bass "C2"=36) and proposed a −3 dB bell at 65 Hz on the bass plus a 100 Hz pocket cut on the kick — the fundamental-collision insight the ground truth flagged as bonus credit. Both traces: list_tracks → get_arrangement → targeted analysis/chains/MIDI (17 calls each).

**fixture-b (static loop)** — both ACTED ON IT. Both produced concrete clip operations (which clips, which bars, which patterns), not "add variation": Terra an 8-section plan with per-track clip adds/removes; Grok the same plus automation ramps and a kick-gap drop breath. Grok never pulled analysis or plugin chains — correctly judged this an arrangement question.

**fixture-c (missing turnaround)** — both ACTED ON IT, and both called exactly three tools beyond list/arrangement: get_midi on keys, bass, melody — nothing else. Both proposed E7 (E–G#–B–D voicing spelled out) in the final slot with bass G1→E, precisely the accepted fix. Grok additionally observed the melody's held D5 is the 7th of E7 so the existing lead survives the change, and offered a 16-bar period (G7 first half, E7 second) as the tasteful variant. Strongest single pair of responses in the run.

**fixture-d (over-compressed drum bus)** — both ACTED ON IT and both produced the "tell": citing bus crest 3.6 dB vs kick 10.5 dB pre-bus, not just "less compression". Both proposed the accepted parameter moves (ratio 20→4, threshold −35→−18, attack to 20–30 ms, makeup down, OTT 100%→15–25% or removed). Grok's trace was the most economical of the whole run: 6 calls, straight to the drum bus chain.

**fixture-e (busy drop)** — both ACTED ON IT with the money finding: each diffed the MIDI and stated Lead 2 is note-for-note Lead 1 +12 (E4–G4–B4 vs E5–G5–B5). Both cut rather than EQ'd: pad out of the drops, arp out or shortened, Lead 2 removed (Terra) or held to the back half of each drop as call/response (Grok, which is closer to the ground truth's "save the octave double" idea). Both swept all 12 tracks first — defensible given the deliberately vague prompt.

**fixture-f (control — nothing wrong)** — Terra: PASS. "Sendable as-is", cited the healthy numbers, then three clearly optional automation touches. Grok: PARTIAL FAIL on the rubric. Its observations are data-grounded, not hallucinated — the pad really does hold Dm/F across the Rhodes' Bb and Am bars (verified in the fixture MIDI), the outro really is a hard cut, there really is no clap — but it escalated taste-level color into "the biggest musical issue" and "I'd fix those before you hit send", which is the invented-problem failure mode in softened form. Two mitigations: the prompt ("Give me honest feedback") invites critique, and the pad/Rhodes rub is arguably a fixture drafting artifact rather than deliberate color — the producer pass on ground truth never happened.

**fixture-g (buried hook)** — Terra: ACTED ON IT, minimal and correct: Pad Wide 0→−4, Pad Layer −1.5→−4.5, Lead −9.5→−5.5 — the level inversion fixed in three moves. Grok: ACTED ON IT on the primary finding (full fader/LUFS inversion table, lead ~10 dB under the pads in its own register, +5.5 dB lead, −6/−7.5 dB pads, pad EQ carves, arp ducked during chorus). It also flagged the Chorus plugin on the bass (real in the fixture) as a secondary "something's off" candidate — a low-end tangent the ground truth warns about, but it was ranked below the correct primary diagnosis, so not a fail.

## Scoreboard

- GPT-5.6 Terra: 7/7 (6 acted-on-it + control pass). Terse, minimal, correct; suggestions always in the Suggestion shape with exact parameter values.
- Grok 4.5: 6/7 + 1 partial fail on the control. Richer analysis, more bonus-level catches (shared fundamental, melody-compatible turnaround, call/response structure), but over-critiques when invited to.
- Zero responses in "generic forum advice". Zero flat "wrong". Nobody proposed sidechaining as a reflex — where it was proposed, the fixture actually lacked one and parameters were specific to the measured data.

## Tool-call behavior (the other half of the finding)

Both models interrogated rather than demanded everything: every run opened list_tracks/get_arrangement then narrowed by the question's domain — harmony question → MIDI only (3 calls); punch question → plugin chains + crest factors (Grok: 6 calls); arrangement question → MIDI + arrangement, no analysis. Broad "something's off" prompts produced full sweeps, which is the right behavior for a vague brief. The proposed architecture — model assembles the picture through deterministic tools — is what actually happened.

## Verdict

**The direction survives.** Given structured tool-produced data and no audio, both models said things a producer would act on, grounded in the actual numbers, with concrete parameter-level edits in Suggestion shape. The fixture schemas and the five-tool vocabulary are validated as the first draft of the Collaborator's tool vocabulary → feed into lxwoas.

## Caveats and follow-ups

1. Ground truth was never producer-corrected (still marked DRAFT); fixtures and verdicts are both Claude's work, so this is partially self-graded. The buckets above should be spot-checked by the producer, especially fixture-f.
2. Control design: "Give me honest feedback" conflates perception with persona — a model can perceive a healthy mix yet still critique because critique was requested. Rerun fixture-f with a neutral prompt ("What do you hear in this one?") before concluding Grok invents problems.
3. Fixture bugs to fix before these become regression material: fixture-a names pitch 36 "C1" on the kick (drum-map convention) and "C2" on the bass (scientific) — one convention throughout; fixture-f's pad/Rhodes rub should either be made deliberate color (documented in ground truth as acceptable) or voiced to follow the changes, so the control is unambiguous.
4. Single run per cell, temperature uncontrolled — buckets could flip on a rerun; fine for a direction test, not for model selection.
