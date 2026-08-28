// What the sidecar was started with.
//
// Its own file because both halves of the entry point read it and neither may
// pull the other in: `main.ts` must stay light enough to answer the socket
// before pi is loaded.

export interface Options {
    socketPath?: string;
    dumpPrompt: boolean;
    promptParameters: Record<string, unknown>;
    offlineScript?: string;
    contextDump?: string;
}

export function parseArguments(argv: string[]): Options {
    const options: Options = { dumpPrompt: false, promptParameters: {} };

    for (let at = 0; at < argv.length; ++at) {
        const argument = argv[at] ?? "";

        if (argument === "--dump-prompt") options.dumpPrompt = true;
        else if (argument === "--params")
            options.promptParameters = JSON.parse(argv[++at] ?? "{}") as Record<string, unknown>;
        else if (argument === "--offline-script") options.offlineScript = argv[++at];
        else if (argument === "--dump-context") options.contextDump = argv[++at];
        else if (!argument.startsWith("--") && options.socketPath === undefined) options.socketPath = argument;
    }

    return options;
}

/** The one way anything in this process says anything to a terminal, and it is
    off unless a developer turned it on.

    The sidecar shares the DAW's stdout and stderr, so a word written to either
    is a word in the DAW's own output (spec js437t: the sidecar is invisible).
*/
export function debug(message: string): void {
    if (Bun.env["DUET_SIDECAR_DEBUG"] === undefined) return;

    process.stderr.write(`duet-sidecar: ${message}\n`);
}
