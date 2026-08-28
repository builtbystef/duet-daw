# The Duet sidecar

The other half of the AI seam (ADR 0003): a headless host that embeds pi's agent
loop, connects to the socket the DAW listens on, and speaks newline-delimited
JSON-RPC 2.0 over it. It ships inside the DAW install as one standalone binary
and the Target Producer never sees it — no window, no output, and its death is
one failed Task Run rather than a DAW that stopped working.

It is built from `@earendil-works/pi-agent-core` and `@earendil-works/pi-ai` and
deliberately **not** from `@earendil-works/pi-coding-agent`: the loop, the
provider abstraction, the auth and the streaming are worth having, and the
coding agent's tools and identity are not (measured at d8k46e). The agent is
constructed with an empty built-in tool set, so the only tools that exist are the
ones in `src/vocabulary.ts`, and every one of them is answered by the DAW across
the socket.

## Building

    bun install
    bun run build      # writes dist/duet-sidecar

The CMake build does both, into `<build>/sidecar/duet-sidecar`, whenever `bun`
is on the PATH; `-D DUET_SIDECAR_ENABLED=OFF` turns it off, and the C++ suite's
sidecar tests skip themselves when the binary is not there.

## Running it by hand

    duet-sidecar <socket-path> [--offline-script <file>] [--dump-context <file>]
    duet-sidecar --dump-prompt [--params <json>]

`--dump-prompt` writes the assembled system prompt and the whole tool list to
stdout, which is how the prompt is asserted on without a provider.

`--offline-script` replaces every provider with a scripted model, which is how a
run, its tool calls and its cancellation are asserted on without a provider — the
mirror of `tests/sidecar_double`, which lets the DAW be asserted on without a
sidecar. Nothing in the DAW passes it. The script is a JSON array of turns, or an
object with `tokensPerSecond` and `steps`:

    [
      { "text": "Let me look.", "toolCall": { "name": "list_tracks", "arguments": {} } },
      { "text": "The kick is doing all the work." }
    ]

A turn may instead be `{ "error": "..." }`, which fails the run the way a
provider that refused does.

## Credentials

Model access — the provider auth UI, the model list and the picker — is issue
i84fbb and is not here yet. Until it lands the sidecar takes whatever pi resolves
from its ambient environment, so `OPENAI_API_KEY`, `ANTHROPIC_API_KEY` and the
rest configure a provider for a run by hand.

## Diagnostics

Nothing is written to stdout or stderr in normal operation, because those streams
are the DAW's. `DUET_SIDECAR_DEBUG=1` in the environment opens stderr for a
developer.
