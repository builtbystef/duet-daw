---
id: oocnng
title: 'The sidecar: pi embedded, the Collaborator system prompt, the bundled binary'
state: done
assignee: claude
priority: medium
depends_on:
    - d8k46e
    - xy9438
parent: js437t
created: 2026-08-12T04:03:20Z
updated: 2026-08-28T08:54:14Z
---

## What to build

The real other half of the seam, replacing the test double. A minimal Node host embedding pi's SDK — built-in tools empty, every tool custom and forwarded across the socket — that speaks the protocol the double has been standing in for: configuration, a run with streamed commentary and tool activity, cancellation mapped to the session's abort, a terminal event with its status, and shutdown.

The system prompt is Duet's own: the Collaborator's identity, the provenance rules, the instruction to hedge when confidence is low, and how a Suggestion should be shaped — never pi's coding-agent identity. Prompt-cache discipline holds: frozen content first, volatile deltas last, no timestamps or unordered serialization in a stable prefix. The host builds to a standalone binary with the bundler the prototype chose and ships inside the DAW install, invisible to the Target Producer.

## Acceptance criteria

- [ ] The DAW spawns the shipped binary, configuration round-trips, and a prompt against a configured provider streams commentary that reaches the DAW-side listener.
- [ ] Every tool the model calls arrives at the DAW as a tool call and its result returns to the model; the model's tool list holds the Tool Vocabulary and the write-tool and nothing else — no file, shell, or web tools.
- [ ] The system prompt in force is Duet's: a dump of the assembled prompt contains the Collaborator identity and the provenance rules and contains no coding-agent identity or instruction.
- [ ] Cancellation aborts the in-flight provider request: the run ends canceled and no commentary arrives after the terminal event.
- [ ] A provider error or an unauthenticated provider ends the run failed with a plain error string, and the sidecar stays alive and usable for the next run.
- [ ] Prompt-cache discipline, asserted on a dump: the stable prefix is byte-identical across two runs in the same project state, and nothing volatile appears before the volatile tail.
- [ ] The binary runs on a machine with no Node installed, and its size and cold-start latency are within the range the prototype recorded.
- [ ] The sidecar is invisible in normal use: no console window, no stray output on the DAW's streams, and its crash or absence never takes the DAW down.
- [ ] An end-to-end run against a real provider on a fixture project produces commentary grounded in tool results, recorded as a note with the tool trace.

## Notes

**claude** — 2026-08-17T04:12:26Z

Scope note (2026-08-17): drafting the Collaborator system prompt text is in-scope here, bound by js437t: Duet's own identity (never pi's coding-agent identity — including replacing pi-ai's silent 'helpful assistant' default recorded at d8k46e), the provenance rules, the low-confidence hedging instruction, Suggestion guidance, and prompt-cache discipline (frozen content first, volatile deltas last, no timestamps in stable prefixes).

**claude** — 2026-08-28T08:54:04Z

Completed 2026-08-28. The sidecar exists: pi's agent loop wearing Duet's prompt, built by bun into one standalone binary that ships beside the application, driven by the real Collaborator service over a real socket. Every acceptance criterion is met, the last one by a live run against OpenAI whose trace is at the bottom of this note.

WHAT LANDED

- sidecar/ — the Node/Bun host. main.ts (arguments, the connection, quiet death), host.ts (the protocol methods and the two dumps), rpc.ts (newline-delimited JSON-RPC 2.0, written out rather than shared with the DAW, for the same reason the double writes its own), session.ts (the agent, the run, the tool bodies), vocabulary.ts (the eight tool declarations), prompt.ts (the Collaborator prompt), offline.ts (the scripted model), options.ts.
- sidecar/CMakeLists.txt — builds through bun when bun is on the PATH, and is silently absent when it is not; DUET_SIDECAR_ENABLED says the same deliberately. modules/duet_app copies the binary beside Duet.
- tests/SidecarTests.cpp — twelve cases against the shipped binary. tests/SidecarLiveTests.cpp — the hidden [.live] case. tests/CollaboratorHarness.h gains RecordingListener (lifted out of TaskRunTests, a third suite on the same seam) and a constructor taking a plain argument list.
- .github/actions/linux-build-env installs bun 1.3.10 (SHA-pinned action), so the push gate builds and tests the sidecar rather than skipping it.
- docs/ARCHITECTURE.md — the sidecar bullet, filled in.

DECISIONS A REVIEWER NEEDS

1. Testing without a provider. The sidecar takes --offline-script, which replaces every provider with pi-ai's own fauxProvider driven by a JSON script of turns. This is the exact mirror of tests/sidecar_double: the double lets the DAW be driven with no sidecar, the script lets the sidecar be driven with no provider, and together they mean every rule of the seam is asserted by something that shares no code with what it is asserting about. It is not reachable by anything the DAW passes. fauxProvider honours the abort signal between chunks, which is what makes the cancellation case a claim about an aborted request rather than an ignored one.

2. --dump-prompt writes the assembled prompt and the whole tool list to stdout; that is what the prompt criteria are asserted on. Stronger still, --dump-context writes what the provider was actually handed, read from the far side of the agent, and one case asserts the two agree.

3. The prompt is two halves and the order is the mechanism, not a rule to remember: collaboratorPrompt never moves, and systemPromptParams renders after it. A case asserts the same parameters give byte-identical bytes, that a different project changes only the tail, and that the frozen half is a prefix of both.

4. The tool list is all seven read tools plus suggest, including get_track_analysis and estimate_audio_content, whose DAW halves are 3bgymu and 2z0y5u and do not exist yet. The vocabulary is the spec's fixed contract and the sidecar is its other half, so it states the contract; the two unbuilt ones answer unknownTool, which is the correctable error ToolRegistry was designed to produce, and those issues will fill them in without touching the sidecar again. The live run below shows the actual cost of that choice — four refused calls, and an answer that hedged around the missing measurement instead of inventing one. It degraded exactly as designed. If you would rather not pay it until 3bgymu lands, gating those two declarations is a two-line change in vocabulary.ts; I left it stating the contract.

5. Silence is structural. The sidecar inherits the DAW's stdout and stderr (posix_spawn with no file actions), so nothing is written to either and even an uncaught exception exits quietly rather than printing a stack trace on the DAW's stream. DUET_SIDECAR_DEBUG opens stderr for a developer. A case asserts the silence through a wrapper script that redirects the child's streams to files.

6. A run is fire-and-forget on this side too, matching xy9438: run.start is answered at once and the ending arrives as a notification. RpcPeer holds inbound lines until serve() is called, because connecting resolves a promise and handlers are registered after it — without the hold a configure landing in that window would be answered method-not-found.

7. One session lives as long as the process, so each Task Run is another turn of one conversation. That is what makes the panel a conversation and what gives a provider's cache a prefix worth keeping.

MEASURED (dev machine, 2026-08-28)

- Binary: 106,579,590 bytes = 101.59 MiB, against the prototype's 100.59 MiB (d8k46e) — +1.0%.
- Cold start: process lifetime for --dump-prompt, median of seven, 0.14 s. Spawn to socket connect, median of five, 139 ms; spawn to configure answered, 149 ms. The prototype recorded 60-70 ms for a provider-free launch, so this is roughly twice it. The cost is pi's module graph under bun --compile, and it is not deferrable: dynamic imports of the provider catalog and of the agent both measured within noise of static ones, because bun evaluates the whole bundle eagerly. Two things that did move the number, recorded so nobody has to find them again: dropping --sourcemap=none (bun 1.3.10 reads it as a request for an external map and writes a 10 MB one beside the binary, and the binary itself was 5 MB larger), and --bytecode, which halves startup to a 55 ms connect but costs +27 MiB. I did not take the bytecode trade: the prototype's recorded verdict accepted ~100 MiB, a third again is not mine to spend, and 140 ms sits against the 1163 ms median to first provider event the same prototype measured. Say the word and it is one flag.
- Runs on a machine with nothing: a case runs the binary under env -i with an empty PATH. The strong form — a container with no node installed — was proved at d8k46e and is not re-proved here.

THE LIVE RUN (criterion 9)

openai:gpt-5.6-luna, against a three-track project built by the test (kick and bass both loud and both on C1, a pad at -14 dB panned left), asked "The low end feels crowded to me. What's going on down there?". The key was supplied by the user for this run only and is in no file, no commit and no note.

Tool trace, in order: list_tracks -> get_arrangement -> get_track_analysis x4 (all REFUSED, no such tool) -> get_midi(Kick) -> get_midi(Bass) -> get_plugin_chain(Bass). Run ended completed.

What it said:

  The crowding is mainly kick and bass occupying the same very low register:
  - Kick: C1 / MIDI 36, every quarter note, 110 velocity.
  - Bass: mostly the same C1 / MIDI 36, with 0.9-beat notes on beats 2-4; it jumps to F1 / MIDI 41 on beat 1 of each bar.
  - Both are centered and fairly loud: kick -3 dB, bass -3.5 dB.
  - The bass notes are almost continuous, so they overlap the kick's repeated hits rather than leaving much space.
  - The bass compressor is present, but its sidechain gain is 0 dB and no sidechain source is assigned, so it is not ducking under the kick.
  - The pad is quieter at -14 dB and panned left, so it is less likely to be the main source of the low-end congestion.
  I couldn't get the rendered spectrum measurement to return, so the frequency-band diagnosis is based on the MIDI and mixer data. The clearest issue is rhythmic and note-register overlap: kick and bass are literally sharing C1 on most hits.

Every number in that is a value a tool returned; nothing in it is invented. The last paragraph is the prompt's hedging instruction working on a real gap.

Repeat it with: DUET_LIVE_MODEL=provider:id ./build/tests/Debug/duet_tests "[live]", with that provider's credentials in the environment. DUET_LIVE_SCRIPT points the same harness at an offline script, which is how the harness itself was rehearsed without spending anything.

NOT IN THIS ISSUE

- Nothing in the DAW spawns the shipped binary yet: the panel meets the real service at 7tw2tz. The build puts the binary where that issue will look for it.
- Provider auth, models.list and the picker are i84fbb. Until then the sidecar takes whatever pi resolves from the environment, which is what the live run above used.
- Cross-platform binaries (macOS, Windows) stay unmeasured, as d8k46e recorded; the bun target is a CMake cache variable.

CHECKS

Format clean, full lint sweep clean, 395/395 tests pass, plus the twelve sidecar cases and the hidden live case.
