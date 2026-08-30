// Everything the sidecar does once it has a connection: the protocol methods,
// the agent session behind them, and the two dumps a test reads.
//
// Separate from `main.ts` so that the entry point stays the small readable thing
// it should be — arguments, the connection, and what to do when the process
// falls over — and the protocol is not read through it.

import { builtinModels } from "@earendil-works/pi-ai/providers/all";
import type { MutableModels } from "@earendil-works/pi-ai";

import { ProviderAccess } from "./access.ts";
import { ProviderAccounts } from "./credentials.ts";
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

/** Every provider pi ships, over the producer's own credentials, and the
    scripted model instead of them all under a script.

    Which of them a producer can actually reach is a question about credentials:
    the store is Duet's own so that a key entered once is there at the next
    launch, and pi resolves anything the environment already carries besides.
*/
async function loadModels(
    options: Options,
): Promise<{ models: MutableModels; accounts: ProviderAccounts; offline?: OfflineProvider }> {
    const accounts = await ProviderAccounts.open(options.credentialsPath);
    const models = builtinModels({ credentials: accounts });

    if (options.offlineScript === undefined) return { models, accounts };

    // The scripted model stands alone: a sidecar under a script must not be able
    // to reach a real provider by accident.
    const offline = await createOfflineProvider(options.offlineScript);
    models.clearProviders();
    models.setProvider(offline.handle.provider);
    models.setProvider(offline.locked);

    return { models, accounts, offline };
}

/** One string member of a request's parameters, and empty when it is missing —
    the methods themselves say what an empty one means.
*/
function text(params: Params, member: string): string {
    const value = params[member];

    return typeof value === "string" ? value : "";
}

/** Registers the protocol methods on a connected peer, and starts answering. */
export function serve(peer: RpcPeer, options: Options): void {
    const loading = loadModels(options);

    /** Model access over the same load `configure` waits on, so the providers a
        key is stored against are the providers a run is configured from: one
        collection, one credential store, one answer to what is authenticated.
    */
    const accessing = loading.then((loaded) => new ProviderAccess(loaded.models, loaded.accounts));

    // A load that failed must reach whoever asks and nobody else: an unobserved
    // rejection takes the process down (main.ts), and a sidecar that cannot read
    // its own credentials should answer the next question rather than die.
    void accessing.catch(() => undefined);
    let offline: OfflineProvider | undefined;

    const session = new CollaboratorSession((call) => peer.request("tool.call", { ...call }), {
        text: (runId, delta) => peer.notify("run.text", { runId, delta }),
        toolActivity: (runId, tool, phase) => peer.notify("run.toolActivity", { runId, tool, phase }),
        finished: (runId, status, error) => {
            writeContextDump(options, offline);
            peer.notify("run.finished", error === undefined ? { runId, status } : { runId, status, error });
        },
    });

    /** The `configure` that is still being answered, which `run.start` waits
        for.

        The DAW sends the two back to back — a run configures the model it is to
        use and starts in the same breath (issue i84fbb) — and answering a
        request is asynchronous here, so without this the run could reach a
        session that is one await away from having a model.
    */
    let configuring: Promise<unknown> = Promise.resolve();

    peer.on("configure", (params: Params) => {
        const model = typeof params["model"] === "string" ? params["model"] : "";
        const raw = params["systemPromptParams"];
        const parameters = typeof raw === "object" && raw !== null ? (raw as Record<string, unknown>) : {};

        configuring = (async () => {
            const loaded = await loading;
            offline = loaded.offline;

            try {
                session.configure(loaded.models, offline !== undefined ? offline.modelId : model, parameters);
            } catch (thrown) {
                throw new RpcFailure(rpcError.invalidParams, thrown instanceof Error ? thrown.message : String(thrown));
            }

            return {};
        })();

        return configuring;
    });

    peer.on("models.list", async () => (await accessing).listing());

    peer.on("auth.setApiKey", async (params: Params) =>
        (await accessing).setApiKey(text(params, "provider"), text(params, "key")).then(() => ({})),
    );

    peer.on("auth.beginOAuth", async (params: Params) => (await accessing).beginOAuth(text(params, "provider")));

    peer.on("auth.completeOAuth", async (params: Params) =>
        (await accessing).completeOAuth(text(params, "provider"), text(params, "code")).then(() => ({})),
    );

    peer.on("auth.remove", async (params: Params) =>
        (await accessing).remove(text(params, "provider")).then(() => ({})),
    );

    peer.on("run.start", async (params: Params) => {
        await configuring.catch(() => undefined);

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
