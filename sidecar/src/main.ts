// The Duet sidecar: pi's agent loop behind the DAW's socket protocol (ADR 0003).
//
// Started by the Collaborator service as a child process, with the path of the
// socket to call home on as its first argument. It connects, answers `configure`,
// `run.start`, `run.cancel`, `models.list`, the four `auth.*` methods and
// `shutdown`, asks `tool.call` whenever the model reaches for a tool, and
// streams a run's commentary, tool activity and ending back as notifications.
//
// Silence is a requirement and not an accident: this process shares the DAW's
// stdout and stderr, so nothing here writes to either. What goes wrong goes back
// over the socket — as a JSON-RPC error, or as a failed run — and what has
// nowhere to go at all goes nowhere, unless DUET_SIDECAR_DEBUG is set, which is
// the developer's way in and no producer's.
//
// Usage:
//   duet-sidecar <socket-path> [--credentials <file>] [--offline-script <file>]
//                              [--dump-context <file>]
//   duet-sidecar --dump-prompt [--params <json>]

import { dumpPrompt, serve } from "./host.ts";
import { debug, parseArguments } from "./options.ts";
import { RpcPeer } from "./rpc.ts";

/** A crash prints a stack trace on the stream it was born with, which here is
    the DAW's. So it does not: the sidecar dies quietly, and the DAW's own rule
    for a sidecar that died takes over from there (xy9438).
*/
function dieQuietly(what: unknown): never {
    debug(`fatal: ${what instanceof Error ? (what.stack ?? what.message) : String(what)}`);
    process.exit(1);
}

process.on("uncaughtException", dieQuietly);
process.on("unhandledRejection", dieQuietly);

async function main(): Promise<void> {
    const options = parseArguments(process.argv.slice(2));

    if (options.dumpPrompt) {
        dumpPrompt(options);

        return;
    }

    const socketPath = options.socketPath;

    if (socketPath === undefined) dieQuietly("expected the path of the socket to connect to");

    serve(await RpcPeer.connect(socketPath), options);
}

void main();
