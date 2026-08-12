---
id: sdfjqh
title: Which AI backends can power the interaction model?
state: done
assignee: claude
priority: high
labels:
    - roadmap:d9gioe
    - session:research
depends_on:
    - hll1mo
parent: d9gioe
created: 2026-08-07T06:02:39Z
updated: 2026-08-07T21:38:15Z
---

Research session. Take the interaction model (node hll1mo) and find what can actually power each capability it demands:

- LLM APIs for session-aware reasoning and tool-calling over the project model (Claude API and peers): capability, latency, cost per session, streaming.
- Music-specific generation: MIDI/symbolic generation, audio generation/stem models — what exists as an API, what runs locally, quality reality-check.
- Local vs cloud: latency budgets compatible with a creative flow, offline story, privacy of unreleased music.
- The wire seam from a C++ app: SDK availability, or plain HTTPS + JSON.

Deliverable: for each capability of the interaction model, the viable backend(s) with cited constraints — enough to pick the first AI interaction honestly.

## Notes

**claude** — 2026-08-07T06:27:08Z

Constraint from the user (2026-08-07): the project starts as open source, with the potential to become a commercial product later. Evaluate every candidate technology, library, and service against that path — prefer permissive or dual-licensable licenses; a copyleft-only option with no commercial-license route (e.g. GPL with no paid tier) forecloses a commercial edition and needs explicit justification. Recorded in the roadmap root (d9gioe) under 'Licensing posture'.

**claude** — 2026-08-07T18:26:47Z

Research session closed (2026-08-07). All claims from primary sources; four parallel sub-agents (LLM APIs, music-specific models, local-vs-cloud, C++ wire seam).

## Question

Which AI backends can power the interaction model settled in hll1mo — a Collaborator that sees the entire project state, consumes rendered audio on demand, and returns commentary and/or a Suggestion (a bundle of structured project edits) from a non-blocking, cancelable, producer-initiated run? Evaluated against the roadmap's licensing posture (open source now, commercial edition possible later).

## Answer

**One frontier LLM API with tool-calling and structured output is the backbone; music-specific generative models are not needed for the first interaction and mostly cannot be shipped commercially anyway.**

The interaction model demands three things of a backend: reason over a large structured project state, emit schema-valid structured edits, and — for feedback tasks — listen to rendered audio. Only **Google Gemini** does all three in one model call: 1M context, audio input at 32 tokens/second (9.5 h max), schema-constrained JSON, and the lowest price of the three majors. Anthropic Claude has **no documented audio input** at all; OpenAI's flagship `gpt-5.6-*` models do not accept audio either — audio lives on a separate `gpt-audio-1.5` with a 128K context and a 16K output ceiling, which cannot hold a large project state and a Suggestion together. So any non-Gemini choice forces a **two-model split**: one call to listen, one call to reason and suggest.

That is not a verdict for Gemini on quality. Gemini downsamples all audio to **16 kbps mono** before the model sees it, which destroys stereo imaging and high-frequency detail — the exact material a mix-critique task depends on. And **no vendor makes any claim that its models reason musically** about pitch, tempo, key, timbre, or mix quality; Gemini's strongest documented statement is that it "understands non-speech sounds (birdsong, sirens, etc.)". Every musical-listening claim in the interaction model is unevidenced by vendor documentation and must be settled by our own listening test before it is designed in. Structured-output guarantees also differ: OpenAI's docs claim outputs "always" adhere to the schema, Anthropic guarantees strict tool inputs validate exactly, and Gemini promises only "syntactically correct JSON" with "always validate values in your application."

**Music-specific generation is a trap for this project's licensing posture, not a shortcut.** The most functionally apt symbolic model — MIDI-GPT, which does bar infilling with per-track density/polyphony/pitch-range constraints, almost exactly the Suggestion shape — ships its weights under **CC-BY-NC-4.0**. So do MusicGen's, madmom's beat/key/chord models, and Open-Unmix's best checkpoint. Essentia is **AGPL-3.0** with CC-BY-NC-ND models. The clean-licence set exists and is real (Anticipatory Music Transformer, Aria, text2midi, ACE-Step, YuE, Beat This, Basic Pitch, Demucs/Spleeter code), but nothing in it is required for a first AI interaction, and every audio-generation option is either non-commercial or heavy (YuE wants 24 GB VRAM minimum).

**Local LLM inference is viable as a second path, not the first one.** llama.cpp is MIT with a C API, ships GBNF grammars that constrain output to a JSON schema, and documents function calling; gpt-oss-20b (Apache-2.0) "can run within 16GB of memory" and an 8B at Q4_K_M is 4.58 GiB. That fits the Target Producer's machine alongside a DAW. But it is a smaller model with weaker tool use, and it is a fallback for offline/privacy, not the capability we design the first interaction against.

**The privacy objection to cloud is weaker than assumed, and the offline objection is stronger.** All three majors state they do not train on paid API inputs by default. But none offers zero retention by default — OpenAI keeps abuse logs 30 days and its ZDR path requires prior approval (not something an open-source project can offer users generically), and Google logs "for a limited period." The **Gemini free tier is explicitly unsuitable**: Google uses unpaid-tier content to develop its products, with human reviewers. A "bring your own key" path must refuse or loudly warn on free-tier keys.

**The wire seam is settled and cheap.** No vendor publishes a C++ SDK — Anthropic lists seven languages, OpenAI six official plus a community list, Google five; C++ is on none of them. So it is plain HTTPS + JSON + SSE, hand-rolled. **JUCE's own `juce::WebInputStream` is not adequate for streaming**: `read()` "will block until the maxBytesToRead bytes are available", there is no read timeout (only a connection timeout), and feeding an SSE parser would mean byte-at-a-time reads on a dedicated thread. Its one advantage is `cancel()`, an explicit out-of-band cancel. **libcurl** (permissive curl licence) is the recommended client: `CURLOPT_WRITEFUNCTION` delivers chunks as they arrive, `curl_mime` handles audio upload, and cancellation is in-band via a callback returning abort — worst case ~1 s latency on an idle stream, since the progress callback fires "about one call per second" when idle. Every candidate library (curl licence, MIT, BSL-1.0, Apache-2.0) is permissive and safe under both the AGPLv3 open-source path and a future paid JUCE licence, whose EULA forbids linking JUCE against copyleft.

## Findings

### Sub-question 1 — LLM APIs for session-aware reasoning and tool-calling

**Context and output.** Anthropic: `claude-opus-5` / `claude-sonnet-5` / `claude-fable-5`, 1M context, 128K output (first-party model catalog, cached 2026-06-24; live via `GET /v1/models`). OpenAI: `gpt-5.6-sol` / `-terra` / `-luna`, 1.05M context, 128K output (https://developers.openai.com/api/docs/models). Google: `gemini-3.6-flash`, `gemini-3.1-pro-preview`, 1,048,576 in / 65,536 out (https://ai.google.dev/gemini-api/docs/models/gemini-3.6-flash).

**Audio input — the decisive axis.** Gemini accepts wav/mp3/aiff/aac/ogg/flac; "Max length: 9.5 hours of audio per prompt"; "32 tokens per second of audio (1 minute = 1,920 tokens)"; inline requests capped at 20 MB total, Files API above that; **"Downsampled to 16 Kbps"** and **"Multi-channel audio combined to single channel"**; "Gemini also understands non-speech sounds (birdsong, sirens, etc.)" (https://ai.google.dev/gemini-api/docs/audio). A 3-minute mix is 5,760 tokens ≈ $0.0086 on 3.6-flash.

OpenAI's flagship models list text+image input only; audio exists on `gpt-audio-1.5` (128K context / 16,384 max output, "understand and generate audio and text") and `gpt-realtime-2.1` (https://developers.openai.com/api/docs/guides/audio). Audio is priced at $32/1M in, $64/1M out. Anthropic documents image, PDF and Files-API document input; **audio is absent from every first-party input-modality list** — an argument from total absence, not an explicit negative.

**Structured output.** OpenAI: Structured Outputs "ensures the model will always generate responses that adhere to your supplied JSON Schema" (`strict: true`); limits ≤10 nesting levels, ≤5,000 properties, ≤1,000 enum values; `allOf`/`not`/`if-then-else` unsupported (https://developers.openai.com/api/docs/guides/structured-outputs). Anthropic: `output_config.format` plus `strict: true` on tools, which "guarantees `tool_use.input` validates exactly"; recursive schemas and numeric/string-length constraints unsupported. Google: output is "syntactically correct JSON" but "always validate values in your application"; supports a JSON Schema subset including `minimum`/`maximum`, which Anthropic drops — relevant if a Suggestion schema constrains MIDI note numbers or dB values (https://ai.google.dev/gemini-api/docs/structured-output).

**Streaming and cancellation.** All three stream over SSE. Only OpenAI has a server-side cancel endpoint, `POST /responses/{id}/cancel`, and "Only responses created with the `background` parameter set to `true` can be cancelled." Anthropic has no cancel endpoint for `/v1/messages`; Gemini documents no cancellation mechanism at all. **For all three, a cancelable run is client-side stream teardown.**

**Prompt caching** (the lever for resending project state). Anthropic: explicit `cache_control` breakpoints, max 4/request, prefix-match, reads ~0.1× / writes 1.25× (5m) or 2× (1h); minimum cacheable prefix is model-dependent and non-monotonic (512 on Opus 5, 1024 on Sonnet 5, 4096 on Opus 4.6/Haiku 4.5). OpenAI: automatic, no code changes; 1,024-token minimum on 5.6+; "A cached prefix remains eligible for reuse for at least 30 minutes" (https://developers.openai.com/api/docs/guides/prompt-caching). Google: implicit caching on by default, 4,096-token minimum on 3.5-flash/3.1-pro; explicit caching costs $1.00/hour storage and is **not supported on the newer Interactions API** (https://ai.google.dev/gemini-api/docs/caching). All three are prefix-match, so the same design constraint applies: **frozen project structure first, volatile deltas last** — a timestamp or an unsorted map serialization in the prefix silently kills every downstream cache hit.

**Pricing ($/1M).** gemini-3.6-flash $1.50 / $7.50 (cached $0.15); gemini-3.5-flash-lite $0.30 / $2.50; gpt-5.6-terra $2.00 / $12.00 (cached $0.20); gpt-5.6-sol $5.00 / $30.00; claude-opus-5 $5.00 / $25.00; claude-sonnet-5 $3.00 / $15.00; claude-haiku-4-5 $1.00 / $5.00; gpt-audio-1.5 audio $32 / $64. Session sanity check (100K-token project state, 20 turns, cache hitting): ~$0.15 Gemini 3.6 Flash, ~$0.40 gpt-5.6-terra, ~$0.50 Opus 5.

**Latency.** **No vendor publishes latency or throughput benchmarks.** All publish control surfaces: OpenAI `reasoning.effort` (`none`…`max`, `none` "for latency-critical tasks") and `service_tier: flex`; Google `thinking_level` (`minimal`…`high`); Anthropic `output_config.effort` plus `speed: "fast"` ("up to 2.5x higher output tokens per second", Opus 5/4.8, Claude API only). OpenAI additionally documents **predicted outputs**, which "significantly reduce latency… when you know most of the output ahead of time, such as code editing tasks" — directly applicable to a revised Suggestion that is mostly identical to the previous one.

### Sub-question 2 — Music-specific generation

**Symbolic / MIDI, clean licences.** Anticipatory Music Transformer — code Apache-2.0, weights `stanford-crfm/music-medium-800k` Apache-2.0, 360M params, does continuation, infilling and accompaniment (https://github.com/jthickstun/anticipation). Aria — Apache-2.0 code and weights, 1B, solo piano, self-documented as best at "continuing existing piano MIDI files rather than generating music from scratch" (https://github.com/EleutherAI/aria). text2midi — Apache-2.0 both, but its own listening study scores below its training set on chord matching (2.50 vs 3.20) and key matching (3.64 vs 4.61) (https://huggingface.co/amaai-lab/text2midi). MidiTok MIT, note-seq/magenta.js Apache-2.0 (note-seq archived read-only 2026-05-06).

**MIDI-GPT — the licence conflict, and it is a finding.** Functionally the closest match to Suggestions: bar infilling that preserves the arrangement, new-track generation, per-track and per-bar attribute control over note density, polyphony, duration, key, pitch range, silence and genre. Code is **MIT** (https://raw.githubusercontent.com/Metacreation-Lab/MIDI-GPT/main/LICENSE). Weights are **CC-BY-NC-4.0** per the Hugging Face model card that actually ships them (https://huggingface.co/Metacreation/MIDI-GPT), while the GitHub README reads as if they are MIT. **Both sides recorded; treat the weights as non-commercial until Metacreation Lab clarifies in writing.**

**Audio generation.** MusicGen/AudioCraft — MIT code, **CC-BY-NC-4.0 weights**; 32 kHz; "requires a GPU with at least 16 GB of memory" for the 1.5B; own model card: cannot generate realistic vocals, "sometimes produces abrupt endings or collapse to silence". Stable Audio Open — Stability AI Community License, commercial use needs a separate paid licence; 47 s stereo 44.1 kHz; "better at generating sound effects and field recordings than music". ACE-Step — **Apache-2.0 code and weights**, 3.5B, 8 GB VRAM with CPU offload, RTX 4090 34× realtime; own limits: "Generations exceeding 5 minutes may lose structural coherence", "vocal synthesis lacks nuance". YuE — Apache-2.0 both, but "80GB+ VRAM… recommended" for full songs, 24 GB minimum. Magenta RealTime 2 — Apache code / CC-BY-4.0 weights, 48 kHz stereo, ~200 ms control latency, but real-time streaming requires Apple Silicon and the authors state quality metrics are "forthcoming". Hosted: Google Lyria on Vertex (184 s max), ElevenLabs `POST /v1/music` (3 s–10 min, structured `composition_plan`), Stability Stable Audio 2.5. **No first-party hosted symbolic/MIDI API was found.**

**Analysis / separation.** Demucs and Spleeter — MIT code, **weights licence not stated anywhere**. Open-Unmix — MIT code, best checkpoint `umxl` is **CC-BY-NC-SA-4.0**. Basic Pitch — Apache-2.0, audio-to-MIDI with pitch bend, downmixes stereo to mono, "performs optimally analyzing one instrument at a time". Beat This — **MIT code and MIT weights**, ~78 MB. **madmom** — BSD source but "models/data files distributed under CC BY-NC-SA 4.0" with an explicit contact-us clause: the useful beat/key/chord models are non-commercial. **Essentia** — **AGPL-3.0** with CC-BY-NC-ND models; AGPL contaminates a linked C++ DAW. Both are landmines to record now.

### Sub-question 3 — Local vs cloud

**Embeddable runtimes.** llama.cpp — **MIT**, `include/llama.h` is a C interface (`extern "C"`), backends CUDA/HIP/Metal/Vulkan/SYCL/OpenCL, documented on Linux, Windows and macOS; the HTTP server is an optional example, not a requirement. Caveat: the header carries many `DEPRECATED()` macros and `[EXPERIMENTAL]` structures — the API is still moving. ONNX Runtime GenAI — MIT, has a C++ API reference, but packages are CPU/DirectML/CUDA and **no OS support matrix is published**; DirectML's prominence reads Windows-leaning, which cuts against Linux-first. ExecuTorch — BSD, C++ `text_llm_runner.h`, "Linux, macOS, Windows", but the C++ LLM page names no backends. MLC LLM documents no C++ embedding API and is not viable for this.

**Structured output locally.** llama.cpp ships **GBNF grammars** with JSON-schema conversion, usable outside the server; important caveat from the project: "The JSON schema is only used to constrain the model output and is not injected into the prompt. The model has no visibility into the schema" — describe it in the prompt too. Function calling is "supported for all models" with native templates for Llama 3.x, Qwen 2.5, Hermes, Mistral Nemo, DeepSeek R1 and others, falling back to a generic format that "may consume more tokens and be less efficient". Also: "Beware of extreme KV quantizations (e.g. `-ctk q4_0`), they can substantially degrade the model's tool calling performance."

**Open-weight licences for a commercial edition.** Apache-2.0 with zero downstream contract surface: **gpt-oss-20b** (OpenAI; "Agentic capabilities: … function calling, web browsing, Python code execution, and Structured Outputs"; 131,072 context) and **Qwen3-8B** (32,768 native, 131,072 with YaRN; "Qwen3 excels in tool calling capabilities"). Shippable but with obligations: **Gemma 3** requires carrying Google's use restrictions into the DAW's own EULA as an enforceable provision, plus a notice file; **Llama 4** requires "Built with Llama" displayed prominently and a Meta licence above 700M MAU. Disqualifying: **Cohere Command A**, CC-BY-NC — "NonCommercial purposes only".

**Hardware.** gpt-oss-20b "can run within 16GB of memory" via MXFP4. llama.cpp's own table for Llama-3.1-8B: Q4_K_M = 4.58 GiB at ~71.93 t/s generation (hardware not stated); the same table shows 70B at 43.1 GB even at Q4_K_M — off the table for a bedroom producer's machine. Qwen publishes Qwen3-4B at 7,973 MB BF16 / 2,915 MB AWQ-INT4 (datacentre GPU; use the memory column, not the throughput column).

**Cloud data handling.** Anthropic: "By default, we will not use your inputs or outputs from our commercial products… to train our models"; thumbs-up/down feedback stores the whole conversation "for up to 5 years" (https://privacy.claude.com/en/articles/7996868). OpenAI: API data "is not used to train or improve OpenAI models"; "abuse monitoring logs are generated for all API feature usage and retained for up to 30 days"; ZDR is "subject to prior approval by OpenAI" (https://developers.openai.com/api/docs/guides/your-data). Google paid tier: "Google doesn't use your prompts… or responses to improve our products", logs kept "for a limited period of time"; **unpaid tier: "Google uses the content you submit… to provide, improve, and develop Google products and services"** with human reviewers and an explicit "Do not submit sensitive, confidential, or personal information" (https://ai.google.dev/gemini-api/terms).

**Offline.** Documented by construction: `llama-cli -m [PATH TO MODEL]` needs no network after a one-time model download. The capability cost is a 4B–20B model with 32K–131K context and grammar-constrained output instead of a 1M-context frontier model — and no audio understanding at all in the local path.

### Sub-question 4 — The C++ wire seam

**No first-party C++ SDK from any vendor.** Anthropic lists Python, TypeScript, C#, Go, Java, PHP, Ruby (https://platform.claude.com/docs/en/api/client-sdks). OpenAI lists six official plus a community list covering eleven more languages, none of them C++ (https://developers.openai.com/api/docs/libraries). Google lists Python, JS/TS, Go, Java and C# (https://ai.google.dev/gemini-api/docs/libraries). All three are absence-from-a-complete-enumeration, not affirmative denials.

**HTTP clients.** libcurl — curl licence, permissive, "inspired by MIT/X"; `CURLOPT_WRITEFUNCTION` "gets called… as soon as there is data received", chunk sizes not guaranteed ("It may be one byte, it may be thousands") so the SSE parser must buffer across calls; `CURLOPT_TIMEOUT_MS` caps the *whole* transfer and **must be left at 0 for long streams**; `curl_mime` is "the recommended way to post an HTTP form" for audio upload. cpp-httplib — MIT, HTTPS only via an external TLS backend chosen at compile time, content-receiver callbacks return `false` to cancel, but "This library uses 'blocking' socket I/O" with thread-per-connection. Boost.Beast — BSL-1.0, but "This library is not a client or server" — you write connection setup, redirects, retries and proxy handling yourself; incremental reads via `buffer_body`.

**JUCE's own networking is not adequate for streaming.** From `juce_WebInputStream.h` (JUCE 9 master): `read()` "will block until the maxBytesToRead bytes are available"; `connect()` blocks until headers arrive; `createInputStream` "must only call it from a background thread"; configuration is limited to extra headers, custom verb, redirects and **`withConnectionTimeout` only — no read/transfer timeout**. Upload does work well: `withFileToUpload` / `withDataToUpload` for multipart, and `Listener::postDataSendProgress` "@returns true to continue or false to cancel the upload". Its one genuine advantage is `cancel()` — "Will cancel a blocking read and prevent any subsequent connection attempts" — the only out-of-band cancel among the candidates.

**JSON.** nlohmann/json MIT with a SAX interface; RapidJSON MIT **except `bin/jsonchecker/` which is under the JSON License ("shall be used for Good, not Evil") — exclude that directory**; simdjson Apache-2.0, whole-buffer/padded input, its "streaming" is NDJSON not partial-document; glaze MIT. Practical shape: **each SSE `data:` line is a complete JSON document, so no resumable JSON parser is needed** — only incremental byte handling plus a line/event assembler. Exception: Anthropic's `input_json_delta` carries partial JSON strings that must be concatenated and parsed only at `content_block_stop`.

**SSE.** Per the WHATWG spec (https://html.spec.whatwg.org/multipage/server-sent-events.html): `text/event-stream`, always UTF-8; lines separated by CRLF, LF **or bare CR — all three must be handled**; field names compared literally with no case folding; blank line dispatches; leading `:` is a keep-alive comment to ignore; multiple `data:` lines join with `\n`. Vendors differ: Anthropic uses **named events** (`event: message_stop`) with no `[DONE]` sentinel and warns "new event types may be added, and your code should handle unknown event types gracefully"; OpenAI Chat Completions is **data-only** and "terminated by a `data: [DONE]` message" — a non-JSON payload that must not be fed to the parser. A client must handle both shapes.

**Cancellation.** libcurl: "You must never share the same handle in multiple threads", so no cross-thread cancel — the documented route is the progress callback returning 1, which fires "about one call per second" when idle, i.e. **worst-case ~1 s cancellation latency on a quiet stream**. cpp-httplib: in-band callback returning `false` only. Beast/Asio: `socket.cancel()` is genuinely out-of-band and immediate, the strongest option, at the cost of writing the whole client. JUCE: `cancel()`, but **no documented thread-safety guarantee**.

**JUCE 9 licensing.** "The JUCE Framework modules are dual-licensed under the AGPLv3 and the commercial JUCE licence" (https://github.com/juce-framework/JUCE/blob/master/LICENSE.md). Tiers: Starter free ≤$20k revenue, Indie $40/user/month ≤$300k, Pro $175/user/month. The risk runs the other way from what you'd expect: the JUCE 9 EULA §2.3 obliges a commercial licensee "not to do anything that could cause or result in the Framework being subject to any open source licence" — **so a future commercial edition must not link JUCE against copyleft**. Every wire-seam candidate above (curl licence, MIT, BSL-1.0, Apache-2.0) is permissive and safe under both paths. OpenSSL 3.x is Apache-2.0; libcurl on Linux may link GnuTLS or OpenSSL depending on the distro package, so pin the TLS backend deliberately.

## Unresolved

- **Whether any model reasons musically about audio.** No vendor claims it. Gemini's "understands non-speech sounds (birdsong, sirens, etc.)" is environmental-sound classification, not music analysis. Blocks any confident design of audio-driven feedback tasks; needs our own eval.
- **What Gemini's 16 kbps mono downsample costs musically.** Documented as a fact, undocumented as an effect. Needs a listening test before mix-critique tasks are specified.
- **OpenAI audio formats, duration limits and audio-token rates** — absent from both the audio guide and the `gpt-audio-1.5` model page.
- **Gemini request cancellation** — no documented mechanism anywhere in the text-generation or streaming docs.
- **Claude audio input** — verified only by exhaustive absence across first-party input documentation, not by an explicit negative statement. `models.retrieve(id).capabilities` is the live check.
- **MIDI-GPT weights licence** — GitHub README and Hugging Face model card disagree (MIT vs CC-BY-NC-4.0). Needs written clarification from Metacreation Lab before any use.
- **Pretrained-weight licences for Demucs, Spleeter, MT3, MusicVAE/GrooVAE checkpoints, and Basic Pitch's weights specifically** — every repository states a code licence and is silent on the checkpoints. The largest open legal question in this report for a commercial edition.
- **Open-Unmix `umxhq` / `umx` / `umxse` weight licences** — only `umxl` is explicitly labelled.
- **ONNX Runtime GenAI OS support matrix** — no first-party page enumerates it.
- **libcurl cross-thread safety mid-transfer** — the thread-safety page forbids concurrent handle use but does not state which functions may be called from another thread during a transfer.
- **JUCE `WebInputStream::cancel()` thread safety** — implied by its purpose, never documented. Read `juce_Network_linux.cpp` before relying on it.
- **`data: [DONE]` for OpenAI's Responses API** (as opposed to Chat Completions) — not confirmed from a primary source.
- **Anthropic general API retention period** — the training page states 5 years only for thumbs-up/down feedback conversations.
- **Magenta RealTime 2 quality metrics** — the authors state the technical report is forthcoming.

## Sources

LLM APIs: platform.claude.com/docs/en/api/client-sdks; developers.openai.com/api/docs/{models,libraries,pricing,guides/structured-outputs,guides/audio,guides/prompt-caching,guides/your-data,api-reference/responses}; ai.google.dev/gemini-api/docs/{models,audio,structured-output,caching,pricing,text-generation,libraries}; ai.google.dev/gemini-api/terms; privacy.claude.com/en/articles/7996868; first-party Anthropic model catalog (claude-api skill, cached 2026-06-24).

Music models: github.com/jthickstun/anticipation; huggingface.co/stanford-crfm/music-medium-800k; github.com/EleutherAI/aria; huggingface.co/amaai-lab/text2midi; github.com/Metacreation-Lab/MIDI-GPT + huggingface.co/Metacreation/MIDI-GPT; github.com/facebookresearch/audiocraft (+ MUSICGEN_MODEL_CARD.md, docs/MUSICGEN.md); huggingface.co/stabilityai/stable-audio-open-{1.0,small}; github.com/Stability-AI/stable-audio-tools; github.com/magenta/{magenta,note-seq,magenta-js,magenta-realtime,mt3}; huggingface.co/google/magenta-realtime{,-2}; github.com/ace-step/ACE-Step + huggingface.co/ACE-Step/ACE-Step-v1-3.5B; github.com/multimodal-art-projection/YuE; github.com/facebookresearch/demucs; github.com/deezer/spleeter; github.com/sigsep/open-unmix-pytorch; github.com/spotify/basic-pitch; github.com/CPJKU/{beat_this,madmom}; github.com/MTG/essentia + essentia.upf.edu/licensing_information.html; github.com/Natooz/MidiTok; docs.cloud.google.com/vertex-ai/generative-ai/docs/models/lyria/*; elevenlabs.io/docs/api-reference/music/compose; stability.ai/news-updates (Stable Audio 2.5).

Local inference: github.com/ggml-org/llama.cpp (README, include/llama.h, docs/build.md, grammars/README.md, docs/function-calling.md, tools/quantize/README.md); github.com/microsoft/onnxruntime-genai + onnxruntime.ai/docs/genai/; github.com/pytorch/executorch + docs.pytorch.org/executorch/stable/; github.com/openvinotoolkit/openvino.genai; llm.mlc.ai/docs/deploy/cli.html; huggingface.co/openai/gpt-oss-20b; huggingface.co/Qwen/Qwen3-8B + qwen.readthedocs.io speed benchmark; ai.google.dev/gemma/{docs/core/model_card_3,terms}; github.com/meta-llama/llama-models llama4/LICENSE; cohere.com/c4ai-cc-by-nc-license.

Wire seam: curl.se/docs/copyright.html + curl.se/libcurl/c/{CURLOPT_WRITEFUNCTION,CURLOPT_READFUNCTION,CURLOPT_TIMEOUT_MS,CURLOPT_CONNECTTIMEOUT_MS,curl_mime_init,threadsafe,CURLOPT_XFERINFOFUNCTION}.html; github.com/yhirose/cpp-httplib; boost.org/doc/libs/latest/libs/beast/* and boost_asio/reference/basic_stream_socket/cancel; JUCE master modules/juce_core/network/{juce_WebInputStream.h,juce_URL.h}; github.com/juce-framework/JUCE/blob/master/LICENSE.md; juce.com/get-juce + juce.com/legal/juce-9-licence; json.nlohmann.me/features/parsing/sax_interface; github.com/Tencent/rapidjson license.txt; github.com/simdjson/simdjson; github.com/stephenberry/glaze; html.spec.whatwg.org/multipage/server-sent-events.html; platform.claude.com/docs/en/docs/build-with-claude/streaming; github.com/openai/openai-openapi openapi.yaml.

**claude** — 2026-08-07T21:38:15Z

**Amendment (2026-08-07) — the brief's premise changed after this report was written.**

The user set two constraints that invalidate part of the verdict above. Read the report through this filter.

**1. The Collaborator is never given audio.** The model receives structured text describing the project — key, tempo, per-track loudness, spectral balance, MIDI as note lists, instrument identity, arrangement structure — produced by deterministic analysis tools the DAW owns and calls. The model does not listen and does not infer musical facts from a rendered mix.

**2. No generative music models.** No MIDI/symbolic generation, no text-to-music, no stem/audio generation, in the first interaction or as a planned capability.

**What this changes in the report:**

- **The vendor verdict is void.** The report recommends Gemini almost entirely because it is the only vendor that takes audio, holds a large project state, and emits schema-constrained JSON in one call. Remove audio input and that argument collapses. Anthropic Claude, OpenAI, and Gemini all become candidates on the remaining axes: context window, tool calling, structured output, streaming, price, latency. The two-model split the report warns about for non-Gemini vendors is no longer forced. **Do not carry "use Gemini" forward as a settled decision.**
- **The two blocking unknowns are retired, not answered.** "Does any model reason musically about audio?" and "what does Gemini's 16 kbps mono downsample cost musically?" no longer gate anything. They are replaced by a different and cheaper unknown: *does an LLM given symbolic/structured music data say anything a producer would act on?* That is testable without a DAW — see the prototype node created for it.
- **The entire music-model section is now reference only.** MIDI-GPT, MusicGen, ACE-Step, YuE and the CC-BY-NC weights minefield are out of scope by decision. The licence findings stay useful if that decision is ever revisited; nothing in that section is on the current path. The *analysis* tools in that section — Beat This, Basic Pitch, MidiTok, and the beat/key/pitch detection landscape — become MORE relevant, not less: they are candidate implementations of the deterministic tool layer the model now depends on. Their licences still need checking against the commercial path (madmom's models are CC-BY-NC-SA-4.0; Essentia is AGPL-3.0 with CC-BY-NC-ND models and would contaminate a linked C++ DAW).

**What survives unchanged:** the wire seam findings (no vendor ships a C++ SDK; `juce::WebInputStream` is inadequate for streaming; libcurl is the recommended transport; each SSE `data:` line is a complete JSON document), the privacy findings (no major trains on paid API input by default; the Gemini free tier is explicitly unsuitable), the local-inference findings (llama.cpp, MIT, C API, GBNF grammars), and every licence finding.

Superseded reasoning, retained above for the record. The new AI data strategy is recorded on the roadmap root (d9gioe).
