---
id: hcxgfv
title: Can an existing agent framework host the Collaborator's loop, and can pi be embedded in a C++/JUCE app?
state: done
priority: high
labels:
    - research
    - roadmap:d9gioe
parent: d9gioe
created: 2026-08-07T21:39:47Z
updated: 2026-08-07T21:40:15Z
---

## Question

Can an existing agent framework host the Collaborator's loop, and specifically: what is the pi coding agent, how does OpenClaw embed it, and can it be embedded in a C++/JUCE application? Prompted by the user's decision (2026-08-07) to drive the Collaborator with deterministic music-analysis tools over structured text rather than audio.

## Answer

pi is a strong, MIT-licensed, well-built agent toolkit — and it is TypeScript on Node/Bun. Embedding it in a C++/JUCE DAW means shipping a second runtime and an IPC hop on every tool call. It buys three things worth having (multi-provider LLM abstraction, a session tree with branching, auto-compaction) and a pile of dead weight (file/bash tools, TUI, coding-agent identity). OpenClaw embeds it in-process only because OpenClaw is itself a Node app; that move does not transfer.

The decisive structural finding: **pi's RPC mode gives the host no way to register tools.** A C++ sidecar host cannot expose `get_midi_clip` back to the agent over the documented protocol. The workaround is a thin TypeScript extension shim using `pi.registerTool()` that forwards to the DAW over a local socket.

Recommendation: **use pi as a throwaway prototyping harness, not as a shipping dependency decision.** It gets a testable agent running in a day. The tool contracts written for the shim transfer unchanged to a native C++ loop, so it is not a one-way door. Defer the ship/don't-ship call until the prototype (node created alongside this one) has said whether structured-text music reasoning works at all.

## Findings

**What pi is.** MIT licence. TypeScript, Node.js/Bun runtime. The repository has moved from `badlogic/pi-mono` to `earendil-works/pi`, and the npm scope from `@mariozechner/*` to `@earendil-works/*` — most third-party writing still uses the old names. Five packages: `pi-ai` ("Unified multi-provider LLM API (OpenAI, Anthropic, Google, etc.)"), `pi-agent-core` ("Agent runtime with tool calling and state management"), `pi-coding-agent` ("Interactive coding agent CLI"), `pi-tui` ("Terminal UI library with differential rendering"), `pi-telemetry`. [README, earendil-works/pi]

**MIT clears the licensing posture.** Permissive, so it works under the open-source edition and a later commercial one. Run as a separate sidecar process it never links against JUCE, so the JUCE EULA §2.3 anti-copyleft clause (node sdfjqh) is not engaged at all. [README]

**Four operation modes.** Interactive TUI; print/JSON (`pi -p`, `--mode json` to "Output all events as JSON lines"); **RPC** (`pi --mode rpc`, "strict LF-delimited JSONL framing", documented as intended for non-Node integrations); and SDK (in-process import of `createAgentSession`). [coding-agent README; docs/rpc.md]

**Built-in tools are wrong for a DAW, and removable.** Defaults are `read, bash, edit, write, grep, find, ls`. `--no-builtin-tools` disables them while keeping extension tools; `--tools <list>` allowlists. [coding-agent README]

**Providers.** Anthropic, OpenAI, Google, Azure, DeepSeek, NVIDIA, Mistral, Groq and others via API keys or subscriptions, plus a llama.cpp router server for local models. This is the single most valuable piece for us — it is the multi-provider abstraction node sdfjqh's wire-seam section says we would otherwise hand-write in C++. [coding-agent README]

**The SDK surface.** `createAgentSession({ sessionManager, modelRuntime, customTools, ... })` returns an `AgentSession` with `prompt(text, options)`, `subscribe(listener)`, `setModel(model)`, `abort()`. Sessions are a tree with `fork()` and `navigateTree()`. Session managers: `SessionManager.inMemory()`, `.create(cwd)`, `.continueRecent(cwd)`. [docs/sdk.md]

**The session tree maps onto Suggestions.** `fork()` plus branching is structurally what node hll1mo's Suggestion + A/B toggle needs — a branch that exists without mutating the trunk until accepted. Non-obvious fit, and the strongest single argument for pi. [docs/sdk.md; node hll1mo]

**How OpenClaw embeds it.** Direct in-process import via `createAgentSession()` rather than subprocess spawning, inside `runEmbeddedAttempt()`. It passes everything through `customTools` and leaves `builtInTools` empty — its own `exec` replaces bash, plus messaging/browser/canvas tools, channel-specific Discord/Telegram/Slack actions, and policy filtering. `subscribeEmbeddedPiSession()` consumes the event stream: `message_start/message_end/message_update`, `tool_execution_start/update/end`, `turn_start/end`, `agent_start/end`, `auto_compaction_start/end`. Then `await session.prompt(...)` and the SDK runs the whole loop. Stated benefits: session lifecycle control, custom tool injection, per-context system prompts, persistence with branching/compaction, auth profile rotation, provider-agnostic model switching. [docs.claw.so/engine/pi/]

**Why that does not transfer.** OpenClaw is a Node application importing a TypeScript library — free. DuetDAW is C++/JUCE; the same move requires bundling Node or Bun cross-platform inside an audio application. For a Linux-first bedroom-producer target (node kimula) that is a material packaging and install cost, not a detail.

**RPC mode has no host-tool mechanism.** The RPC spec exposes `prompt`, `get_commands`, extension-UI request/response dialogs, and an event stream. Tool execution is entirely agent-side; there is no documented interface for the client process to expose callable tools to the agent. RPC also uses `takeOverStdout()` to redirect stray stdout writes so only valid JSONL reaches the host. [docs/rpc.md]

**Extensions can register tools, and that is the documented seam.** Extensions are TypeScript modules loaded via jiti (no build step), auto-discovered from `~/.pi/agent/extensions/`, `.pi/extensions/`, or custom paths in `settings.json`, and loadable ad hoc with `-e`. They "can subscribe to lifecycle events, register custom tools callable by the LLM, add commands, and more", running with full permissions in Node. API: `pi.registerTool({ name, description, parameters: Type.Object({...}), async execute(toolCallId, params, signal, onUpdate, ctx) })`. Lifecycle hooks via `pi.on()` include `session_start`, `tool_call` (can block with `{ block: true }`), `tool_result` (can modify results), `before_agent_start`, `session_shutdown`. The `tool_call` block hook is a natural mount point for the Suggestion approval gate. [docs/extensions.md]

**MCP is not first-party in pi.** MCP arrives through community adapters (`pi-mcp-adapter`, `pi-codemode-mcp`) reading `~/.pi/agent/mcp.json` and `<cwd>/.pi/mcp.json`; an upstream issue requesting an MCP extension example (#563) is open. So the "expose DAW tools as an MCP server" route depends on third-party glue on the pi side. [issue #563; pi-mcp-adapter]

**No official C++ MCP SDK.** Anthropic's SDK list does not include C++; the C++ options are community (`cpp-mcp`, `gopher-mcp`, `peppemas/mcp_server`). [modelcontextprotocol.io/docs/sdk]

**No native C++ agent framework exists.** pi, Claude Agent SDK, OpenAI Agents SDK and LangGraph are all TypeScript or Python. "Embed an agent framework" always means "ship a second runtime." The alternative is a native loop, which given the tool-mediated design is modest: POST `/v1/messages`, read `tool_use` blocks, dispatch to native analyzers, append `tool_result`, repeat — over the libcurl seam already established in node sdfjqh.

**ACP is the standard for this shape, and pi does not speak it yet.** The Agent Client Protocol is JSON-RPC 2.0 over stdio (or HTTP), splitting roles so the client owns UI, permissions and workspace mediation while the agent owns the inference loop and tool calls — which is precisely the DAW-host/agent split here. `session/new` declares the MCP servers the agent should connect to, wiring ACP and MCP in one handshake. pi support is an open upstream discussion (#4444), not a feature. Worth tracking; not actionable now. [agentclientprotocol.com; earendil-works/pi discussion #4444]

## Unresolved

- Whether `pi-agent-core` can be used on its own — without `pi-coding-agent`'s built-in tools and coding-agent system prompt — was not confirmed from primary docs. `packages.md` describes only peer-dependency rules, not per-layer APIs. Reading the source would settle it, and it matters: core-only would be a much lighter dependency than the full coding agent.
- Standalone binary distribution is referenced (`scripts/build-binaries.sh --platform linux-x64`, versioned source archives with SHA256 at GitHub releases) but the exact runtime footprint of a bundled pi — disk size, startup latency, whether Node ships inside it — was not established. This is the number that decides whether sidecar-in-a-DAW is tolerable.
- Minimum Node/Bun version was not pinned down from the README.
- Latency of the full round trip (C++ → sidecar → LLM → sidecar → C++) is unmeasured, and node hll1mo's non-blocking task-run model means it may not matter much. Untested either way.
- Whether pi's session `fork()` semantics genuinely fit Suggestions, or only appear to. Needs a prototype, not more reading.
- Licences of the community MCP C++ implementations were not checked against the commercial path.

## Sources

- https://github.com/badlogic/pi-mono (repo README; redirects to earendil-works/pi)
- https://github.com/badlogic/pi-mono/blob/main/packages/coding-agent/README.md
- https://github.com/badlogic/pi-mono/blob/main/packages/coding-agent/docs/sdk.md
- https://github.com/badlogic/pi-mono/blob/main/packages/coding-agent/docs/rpc.md
- https://raw.githubusercontent.com/badlogic/pi-mono/main/packages/coding-agent/docs/extensions.md
- https://github.com/badlogic/pi-mono/blob/main/packages/coding-agent/docs/packages.md
- https://docs.claw.so/engine/pi/ (OpenClaw Pi integration architecture)
- https://github.com/earendil-works/pi/issues/563 (MCP extension example, open)
- https://github.com/earendil-works/pi/discussions/4444 (ACP support, open)
- https://github.com/nicobailon/pi-mcp-adapter
- https://modelcontextprotocol.io/docs/sdk
- https://github.com/hkr04/cpp-mcp
- https://agentclientprotocol.com/get-started/introduction
