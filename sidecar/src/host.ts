// Everything the sidecar does once it has a connection: the protocol methods,
// the agent session behind them, and the two dumps a test reads.
//
// Separate from `main.ts` so that the entry point stays the small readable thing
// it should be — arguments, the connection, and what to do when the process
// falls over — and the protocol is not read through it.

import { builtinModels } from "@earendil-works/pi-ai/providers/all";
import type { Models } from "@earendil-works/pi-ai";

import { createOfflineProvider, type OfflineProvider } from "./offline.ts";
import { debug, type Options } from "./options.ts";
import { assemblePrompt } from "./prompt.ts";
import { RpcFailure, rpcError, type Params, type RpcPeer } from "./rpc.ts";
import { CollaboratorSession, type OpeningContext } from "./session.ts";
import { vocabulary } from "./vocabulary.ts";

/** How long the answer to `shutdown` has to reach the DAW before the process
    goes. The DAW waits for that answer, so leaving before it is written would
    make an orderly stop indistinguishable from a crash.
*/
const shutdownGraceMilliseconds = 20;

/** What the model is given, written to stdout as JSON: the prompt in force and
    the whole tool list, and nothing else in either.

    This is what the prompt criteria are asserted on — that the identity and the
    provenance rules are in it, that no coding-agent instruction is, that the
    tool list is the Tool Vocabulary and the write-tool, and that the frozen half
    does not move when the volatile half does.
*/
export function dumpPrompt(options: Options): void {
    const dump = {
        systemPrompt: assemblePrompt(options.promptParameters),
        tools: vocabulary.map((tool) => ({
            name: tool.name,
            description: tool.description,
            parameters: tool.parameters,
        })),
    };

    process.stdout.write(`${JSON.stringify(dump, null, 2)}\n`);
}

/** Every provider pi ships, and the scripted model instead of them all under a
    script.

    Which of them a producer can actually reach is a question about credentials,
    and credentials are issue i84fbb's: until it lands, what configures a
    provider is whatever pi resolves from the environment.
*/
async function loadModels(options: Options): Promise<{ models: Models; offline?: OfflineProvider }> {
    const models = builtinModels();

    if (options.offlineScript === undefined) return { models };

    // The scripted model stands alone: a sidecar under a script must not be able
    // to reach a real provider by accident.
    const offline = await createOfflineProvider(options.offlineScript);
    models.clearProviders();
    models.setProvider(offline.handle.provider);

    return { models, offline };
}

/** Registers the protocol methods on a connected peer, and starts answering. */
export function serve(peer: RpcPeer, options: Options): void {
    const loading = loadModels(options);
    let offline: OfflineProvider | undefined;

    const session = new CollaboratorSession((call) => peer.request("tool.call", { ...call }), {
        text: (runId, delta) => peer.notify("run.text", { runId, delta }),
        toolActivity: (runId, tool, phase) => peer.notify("run.toolActivity", { runId, tool, phase }),
        finished: (runId, status, error) => {
            writeContextDump(options, offline);
            peer.notify("run.finished", error === undefined ? { runId, status } : { runId, status, error });
        },
    });

    peer.on("configure", async (params: Params) => {
        const model = typeof params["model"] === "string" ? params["model"] : "";
        const raw = params["systemPromptParams"];
        const parameters = typeof raw === "object" && raw !== null ? (raw as Record<string, unknown>) : {};
        const loaded = await loading;
        offline = loaded.offline;

        try {
            session.configure(loaded.models, offline !== undefined ? offline.modelId : model, parameters);
        } catch (thrown) {
            throw new RpcFailure(rpcError.invalidParams, thrown instanceof Error ? thrown.message : String(thrown));
        }

        return {};
    });

    peer.on("run.start", (params: Params) => {
        const runId = typeof params["runId"] === "string" ? params["runId"] : "";
        const prompt = typeof params["prompt"] === "string" ? params["prompt"] : "";
        const context = params["openingContext"] as OpeningContext | undefined;

        if (runId.length === 0) throw new RpcFailure(rpcError.invalidParams, "a run says which one it is in its runId");

        if (session.activeRunId !== undefined)
            throw new RpcFailure(rpcError.runAlreadyActive, `run ${session.activeRunId} is still going`);

        session.startRun(runId, prompt, context);

        return {};
    });

    peer.on("run.cancel", (params: Params) => {
        session.cancel(typeof params["runId"] === "string" ? params["runId"] : "");

        return {};
    });

    peer.on("shutdown", () => {
        setTimeout(() => {
            peer.close();
            process.exit(0);
        }, shutdownGraceMilliseconds);

        return {};
    });

    // A DAW that went away takes the sidecar with it. Nothing here outlives the
    // process that spawned it, and a stranded sidecar would hold a connection
    // open to nobody.
    peer.whenClosed(() => {
        debug("the DAW closed the connection");
        process.exit(0);
    });

    peer.serve();
}

/** In offline mode, what the provider was actually handed — the strongest form
    of "the system prompt in force", because it is read from the far side of the
    agent rather than from the state the agent was built with.
*/
function writeContextDump(options: Options, offline: OfflineProvider | undefined): void {
    const path = options.contextDump;
    const context = offline?.context();

    if (path === undefined || context === undefined) return;

    void Bun.write(
        path,
        JSON.stringify(
            {
                systemPrompt: context.systemPrompt ?? "",
                tools: (context.tools ?? []).map((tool) => ({ name: tool.name, description: tool.description })),
                messages: context.messages,
            },
            null,
            2,
        ),
    );
}
