// Model access: what the producer can reach, and how they come to reach it.
//
// The four `auth.*` methods and `models.list` of the protocol (ADR 0003), over
// pi's provider layer. Nothing here knows what a picker looks like — it answers
// what is set up, what is authenticated now, and what a sign-in wants next, and
// the DAW draws the rest.
//
// A provider contributes its models to the list once it is *set up*: the
// producer stored a credential for it, or its ambient environment already
// authenticates it. pi ships forty providers and some twelve hundred models, and
// a picker holding all of them is not a picker. Providers come in pi's own
// registration order and models in the provider's own — Duet imposes no order,
// which is what "no provider is privileged" means (spec js437t).

import type { AuthEvent, AuthInteraction, AuthPrompt, Models } from "@earendil-works/pi-ai";

import type { ProviderAccounts } from "./credentials.ts";
import { debug } from "./options.ts";
import { RpcFailure, rpcError } from "./rpc.ts";

/** One model the producer can pick, as `models.list` reports it. */
interface ListedModel {
    /** `provider:model` — what `configure` takes, and what the DAW stores. */
    id: string;
    name: string;
    provider: string;
    providerName: string;
    authenticated: boolean;
}

/** One provider the producer can set up, as `models.list` reports it. The setup
    surface needs these before there is a credential for any model to come from.
*/
interface ListedProvider {
    id: string;
    name: string;

    /** Whether it takes an API key, and whether it has a subscription sign-in. */
    apiKey: boolean;
    oauth: boolean;

    /** Whether the producer has set it up, and whether its auth resolves now. */
    configured: boolean;
    authenticated: boolean;
}

/** What the DAW shows while a subscription sign-in is open. */
export interface OAuthStep {
    url: string;
    instructions: string;
}

/** What the sidecar says when a flow offered no words of its own. */
const defaultInstructions =
    "Open this address in a browser and sign in. If the browser is on another machine, paste the code it ends on back here.";

/** One sign-in in progress: the flow runs on its own while the producer is in a
    browser, and this is everything anybody else needs to reach it.

    The two ways it can end are both here — pi's flows race a local callback
    server against a pasted code, so the code the DAW sends may arrive before the
    prompt that wants it, or never, because the browser came home first.
*/
class OAuthFlow {
    /** Resolved with the address as soon as the flow says what it is. */
    readonly opened: Promise<OAuthStep>;

    /** Resolved when the flow has finished, either way. */
    readonly finished: Promise<void>;

    private announce!: (step: OAuthStep) => void;
    private complete!: () => void;
    private fail!: (failure: Error) => void;

    /** A code the DAW sent that no prompt has asked for yet, and the prompt that
        is waiting for one. Never both.
    */
    private held: string | undefined;
    private waiting: ((code: string) => void) | undefined;

    /** How it ended, and nothing while it is still going. */
    private outcome: "signedIn" | Error | undefined;

    constructor() {
        let announceUrl: (step: OAuthStep) => void = () => undefined;
        let failed: (failure: Error) => void = () => undefined;

        this.opened = new Promise<OAuthStep>((resolve, reject) => {
            announceUrl = resolve;
            failed = reject;
        });

        this.finished = new Promise<void>((resolve, reject) => {
            this.complete = resolve;
            this.fail = (failure) => {
                failed(failure);
                reject(failure);
            };
        });

        this.announce = announceUrl;

        // Nobody may wait on these before they are handed a reason to; an
        // unobserved rejection would take the process down (main.ts).
        void this.opened.catch(() => undefined);
        void this.finished.catch(() => undefined);
    }

    /** The interaction pi's flow drives: the address goes to the DAW, and every
        prompt the flow raises is answered from what the producer pastes back.
    */
    interaction(): AuthInteraction {
        return {
            prompt: (prompt: AuthPrompt) => this.answer(prompt),
            notify: (event: AuthEvent) => this.heard(event),
        };
    }

    /** What the DAW pasted. Answers the prompt waiting for it, or is held for
        the prompt that has not arrived yet.
    */
    supply(code: string): void {
        const waiting = this.waiting;

        if (waiting === undefined) {
            this.held = code;

            return;
        }

        this.waiting = undefined;
        waiting(code);
    }

    succeeded(): void {
        this.outcome = "signedIn";
        this.complete();
    }

    failed(failure: Error): void {
        this.outcome = failure;
        this.fail(failure);
    }

    /** What it ended as, for a code that arrives after it already ended: the
        producer's browser may have come home first, which is a sign-in that
        worked and not a code nobody wanted.
    */
    get ended(): "signedIn" | Error | undefined {
        return this.outcome;
    }

    private heard(event: AuthEvent): void {
        if (event.type !== "auth_url") {
            debug(`sign-in: ${event.type}`);

            return;
        }

        this.announce({ url: event.url, instructions: event.instructions ?? defaultInstructions });
    }

    private answer(prompt: AuthPrompt): Promise<string> {
        const held = this.held;

        if (held !== undefined) {
            this.held = undefined;

            return Promise.resolve(held);
        }

        return new Promise<string>((resolve, reject) => {
            this.waiting = resolve;
            prompt.signal?.addEventListener("abort", () => reject(new Error("that step of the sign-in was abandoned")), {
                once: true,
            });
        });
    }
}

export class ProviderAccess {
    private readonly flows = new Map<string, OAuthFlow>();

    constructor(
        private readonly models: Models,
        private readonly accounts: ProviderAccounts,
    ) {}

    /** `models.list`: every provider Duet can offer, and the models of the ones
        that are set up.
    */
    async listing(): Promise<{ models: ListedModel[]; providers: ListedProvider[] }> {
        const setUp = new Set(this.accounts.setUp());
        const providers = this.models.getProviders();

        // Concurrently, because a check is a read of the environment or of the
        // store and there are forty of them.
        const authenticated = await Promise.all(
            providers.map((provider) =>
                this.models
                    .checkAuth(provider.id)
                    .then((check) => check !== undefined)
                    .catch(() => false),
            ),
        );

        const listedModels: ListedModel[] = [];
        const listedProviders: ListedProvider[] = [];

        providers.forEach((provider, at) => {
            const authed = authenticated[at] ?? false;
            const configured = setUp.has(provider.id) || authed;

            listedProviders.push({
                id: provider.id,
                name: provider.name,
                apiKey: provider.auth.apiKey !== undefined,
                oauth: provider.auth.oauth !== undefined,
                configured,
                authenticated: authed,
            });

            if (!configured) return;

            for (const model of provider.getModels())
                listedModels.push({
                    id: `${provider.id}:${model.id}`,
                    name: model.name ?? model.id,
                    provider: provider.id,
                    providerName: provider.name,
                    authenticated: authed,
                });
        });

        return { models: listedModels, providers: listedProviders };
    }

    /** `auth.setApiKey`: the producer's own key for one provider. */
    async setApiKey(providerId: string, key: string): Promise<void> {
        const provider = this.known(providerId);

        if (provider.auth.apiKey === undefined)
            throw new RpcFailure(rpcError.invalidParams, `${provider.name} does not take an API key.`);

        if (key.trim().length === 0)
            throw new RpcFailure(rpcError.invalidParams, `Enter a ${provider.name} API key, or remove the one stored.`);

        await this.store(providerId, async () => ({ type: "api_key", key: key.trim() }));
    }

    /** `auth.remove`: the credential goes; the provider stays set up, and its
        models stay in the picker unusable.
    */
    async remove(providerId: string): Promise<void> {
        this.known(providerId);
        this.flows.delete(providerId);

        try {
            await this.models.logout(providerId);
        } catch (thrown) {
            throw new RpcFailure(rpcError.internalError, describe(thrown));
        }
    }

    /** `auth.beginOAuth`: starts the flow and answers as soon as it says where
        the producer should go. The flow goes on running while they are there.
    */
    async beginOAuth(providerId: string): Promise<OAuthStep> {
        const provider = this.known(providerId);

        if (provider.auth.oauth === undefined)
            throw new RpcFailure(
                rpcError.invalidParams,
                `${provider.name} has no subscription sign-in — it takes an API key.`,
            );

        const flow = new OAuthFlow();
        this.flows.set(providerId, flow);

        this.models
            .login(providerId, "oauth", flow.interaction())
            .then(async () => {
                await this.accounts.remember(providerId);
                flow.succeeded();
            })
            .catch((thrown: unknown) => flow.failed(new Error(describe(thrown))));

        try {
            return await Promise.race([flow.opened, flow.finished.then(() => neverOpened(provider.name))]);
        } catch (thrown) {
            this.flows.delete(providerId);

            throw new RpcFailure(rpcError.internalError, describe(thrown));
        }
    }

    /** `auth.completeOAuth`: what the producer pasted, and the flow's answer.

        It waits for the sign-in to end, so that the DAW learns at once whether
        the producer is signed in — a code that was wrong says so here rather
        than at the next Task Run.
    */
    async completeOAuth(providerId: string, code: string): Promise<void> {
        const flow = this.flows.get(providerId);

        if (flow === undefined)
            throw new RpcFailure(rpcError.invalidParams, "No sign-in is waiting for a code. Start one again.");

        // The callback in the browser may have finished it already, in which
        // case the code is late rather than wrong.
        const ended = flow.ended;

        if (ended !== undefined) {
            this.flows.delete(providerId);

            if (ended === "signedIn") return;

            throw new RpcFailure(rpcError.internalError, ended.message);
        }

        flow.supply(code);

        try {
            await flow.finished;
        } catch (thrown) {
            throw new RpcFailure(rpcError.internalError, describe(thrown));
        } finally {
            this.flows.delete(providerId);
        }
    }

    private known(providerId: string) {
        const provider = this.models.getProvider(providerId);

        if (provider === undefined)
            throw new RpcFailure(rpcError.invalidParams, `Duet knows no provider called ${providerId}.`);

        return provider;
    }

    private async store(providerId: string, credential: Parameters<ProviderAccounts["modify"]>[1]): Promise<void> {
        try {
            await this.accounts.modify(providerId, credential);
        } catch (thrown) {
            throw new RpcFailure(rpcError.internalError, `That could not be stored: ${describe(thrown)}`);
        }
    }
}

/** A flow that finished without ever saying where to go: whatever went wrong,
    the producer cannot be sent anywhere, and that is what the DAW is told.
*/
function neverOpened(providerName: string): never {
    throw new Error(`The ${providerName} sign-in did not start.`);
}

function describe(thrown: unknown): string {
    return thrown instanceof Error ? thrown.message : String(thrown);
}
