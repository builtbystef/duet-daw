---
id: d8k46e
title: 'Prototype: the sidecar as a standalone binary — footprint, startup, headless pi'
state: done
priority: high
labels:
    - session:prototype
parent: js437t
created: 2026-08-12T04:00:53Z
updated: 2026-08-12T05:58:34Z
---

## What to build

The spec's one unmeasured number, settled before the sidecar is committed to. A disposable prototype that embeds pi's SDK in a headless Node host outside Electron — built-in tools empty, one custom tool is enough — bundles it to a standalone binary, and measures what that costs: disk footprint and cold-start latency to first token. It also settles the question left open at hcxgfv: whether the loop can be driven from the lighter core package alone, without the coding agent's built-in tools and system prompt.

The deliverable is a note with the numbers and a recommendation: keep the sidecar, or exercise the native-loop escape hatch that ADR 0003 preserves. The work stays on a prototype branch.

## Acceptance criteria

- [ ] A headless Node host embeds pi and runs one prompt with one custom tool, streaming text and tool calls to stdout — no Electron, no browser, no window.
- [ ] The built-in tools are provably absent: a dump of the assembled request shows the custom tool and nothing else.
- [ ] The host builds to a single standalone binary that runs on a machine with no Node installed.
- [ ] Recorded as a note: binary size, unpacked install footprint, and cold-start latency to first token as the median of five runs, with the bundler named.
- [ ] Recorded as a note: whether the core package alone can drive the loop without the coding agent's tools and prompt, and what that changes in footprint.
- [ ] The note ends with a recommendation — keep the sidecar or take the native-loop escape hatch — justified by those numbers.
- [ ] The work stays on a prototype branch; nothing merges to main.
- [ ] Closure waits for user review.

## Notes

**agent** — 2026-08-12T05:15:32Z

## Question

Can Duet ship a headless pi sidecar as a standalone binary at an acceptable footprint and cold-start cost, using the lighter core package without the coding agent's built-in tools or system prompt?

## Answer

Yes. Keep the sidecar, built from `@earendil-works/pi-agent-core` and `@earendil-works/pi-ai` 0.84.1; do not include `@earendil-works/pi-coding-agent`. The user accepted a 100.59 MiB single-file binary because its measured local startup is only 60–70 ms and provider latency dominates the interaction. The stable artifact is on branch `prototype/pi-sidecar-footprint` at commit `2a20ef8`.

## Findings

- The working headless host ran one prompt outside Electron, streamed one `inspect_project` tool call and result plus final text to stdout, and exposed the assembled agent and provider requests. Both dumps contained exactly that one custom tool and no built-in tools.
- Core alone drives the loop. `Agent` received an explicit empty system prompt and the one custom tool. No coding-agent identity or instruction appeared. The OpenAI provider adapter substituted its generic `You are a helpful assistant.` instruction for an empty prompt; this belongs to pi-ai, not pi-coding-agent, and production will replace it with Duet's Collaborator prompt.
- Bundler: Bun 1.3.10, using pi's first-party standalone mechanism, `bun build --compile --target=bun-linux-x64-baseline`. The core artifact was one 105,477,040-byte file (100.59 MiB), with no adjacent runtime payload.
- The executable ran in `debian:bookworm-slim` after explicitly asserting that `node` was absent. It reported its embedded Bun runtime and dumped the request successfully.
- Five fresh processes using `openai-codex:gpt-5.6-luna` produced first provider stream events at 1016.95, 1088.18, 1163.57, 1178.99, and 1594.90 ms: median 1163.57 ms. Each first event was a streamed tool-call delta.
- Final text after the tool call and second provider turn arrived at 2085.76, 2262.45, 2342.60, 2407.38, and 2932.75 ms: median 2342.60 ms. This is end-to-end model/network/tool-loop latency, not sidecar startup.
- Five provider-free launches completed in 0.06, 0.06, 0.06, 0.07, and 0.07 seconds. This coarse measurement puts local process startup at 60–70 ms.
- Isolated production dependency closures installed with npm `--omit=dev --ignore-scripts`: core 86,252,303 bytes (82.26 MiB, 17,377 files); coding-agent 129,898,954 bytes (123.88 MiB, 19,090 files). Coding-agent adds 43,646,651 bytes (41.62 MiB).
- With identical Bun compile flags, the working core host was 105,477,040 bytes; a coding-agent SDK import comparator was 120,434,363 bytes (114.86 MiB), 14,957,323 bytes (14.26 MiB) larger. The coding-agent artifact is an import-floor comparator, not a functionally equivalent host; the install-closure comparison is the stronger dependency result.
- Recommendation and accepted verdict: keep the core-only sidecar. Its local startup is negligible beside provider latency, its single-file footprint is acceptable for this desktop DAW, and pi-agent-core plus pi-ai retain the valuable agent loop, auth, provider abstraction, and streaming without coding tools, terminal UI, or coding-agent identity.

## Unresolved

- Cross-platform executable sizes and startup times remain unmeasured; production distribution work must measure macOS and Windows artifacts.
- Peak resident memory was outside this issue's acceptance criteria and remains unmeasured.
- The xAI credential completed three exploratory runs, then its refresh token was reported revoked; the accepted five-run series used the configured OpenAI Codex credential instead.

## Sources

- Pi agent-core 0.84.1 package and API: https://github.com/earendil-works/pi/blob/v0.84.1/packages/agent/package.json and https://github.com/earendil-works/pi/blob/v0.84.1/packages/agent/README.md
- Pi `Agent` initialization and request context: https://github.com/earendil-works/pi/blob/v0.84.1/packages/agent/src/agent.ts
- Pi coding-agent 0.84.1 dependency manifest: https://github.com/earendil-works/pi/blob/v0.84.1/packages/coding-agent/package.json
- Pi standalone build script: https://github.com/earendil-works/pi/blob/v0.84.1/scripts/build-binaries.sh
- Bun compiled executables: https://bun.sh/docs/bundler/executables
- Prototype branch: `prototype/pi-sidecar-footprint`, commit `2a20ef8`

Closure waits for review. Close `d8k46e` to approve this result, or note requested changes and remove `needs-review`.

**agent** — 2026-08-12T05:58:27Z

User confirmed the prototype result and recommendation were already approved; the remaining needs-review state was stale.
