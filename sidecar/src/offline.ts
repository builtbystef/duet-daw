// The sidecar with the provider taken out: a scripted model, so that everything
// between the socket and the agent loop can be driven with no network, no
// credentials and no cost.
//
// This is the counterpart of `tests/sidecar_double`. That double lets the DAW's
// service be exercised with no sidecar; this lets the sidecar be exercised with
// no provider — the same trick from the other end, and together they mean every
// rule of the seam is asserted by something that shares no code with what it is
// asserting about.
//
// It is reached only by `--offline-script <file>`, which nothing in the DAW ever
// passes: a sidecar started the ordinary way registers the real providers and
// this file's provider does not exist. The model underneath is `fauxProvider`,
// which pi-ai ships for exactly this and which honours the abort signal between
// chunks — which is what makes a cancellation test a real one.

import {
    type ApiKeyCredential,
    type Context,
    type Model,
    type OAuthCredential,
    type Provider,
    type ProviderAuthInteraction,
    fauxAssistantMessage,
    fauxProvider,
    fauxText,
    fauxToolCall,
    type FauxContentBlock,
    type FauxProviderHandle,
    type FauxProviderState,
    type SimpleStreamOptions,
} from "@earendil-works/pi-ai";

/** One turn of a scripted conversation: what the model "says" that turn. */
interface OfflineStep {
    /** Commentary, streamed as text deltas. */
    text?: string;

    /** A tool the model calls this turn. The loop calls it and comes back for
        the next step.
    */
    toolCall?: { name: string; arguments?: Record<string, unknown> };

    /** A provider failure, reported the way a real refusal is. */
    error?: string;
}

interface OfflineScript {
    /** How slowly the text streams. Low enough and a cancel lands mid-stream. */
    tokensPerSecond?: number;
    steps: OfflineStep[];
}

/** The scripted model, and what the provider saw when it was asked. */
export interface OfflineProvider {
    handle: FauxProviderHandle;
    modelId: string;

    /** The scripted model's twin, which nothing authenticates by itself: the
        provider the `auth.*` methods are driven against, so that setting a key,
        signing in and removing a credential are all assertable with no network,
        no account and no cost. The scripted provider itself stays authenticated
        by construction, which is what every run test needs of it.
    */
    locked: Provider;
    lockedModelId: string;

    /** The context of the most recent request: the system prompt and the tool
        list exactly as the provider received them, and the transcript as it
        stood — which by the end of a run holds the tool results the DAW sent
        back, so a test can see that they reached the model.
    */
    context(): Context | undefined;
}

export const offlineProviderId = "duet-offline";
export const offlineModelId = "scripted";

/** The locked twin: a provider with nothing ambient about it, whose sign-in
    hands back a fixed address and takes one fixed code.
*/
export const lockedProviderId = "duet-locked";
export const lockedModelId = "padlock";
export const lockedAuthorizeUrl = "https://example.invalid/duet/authorize?client=duet";
export const lockedInstructions = "Sign in there, then paste the code it gives you.";
export const lockedCode = "duet-offline-code";

/** Reads a script and registers the model it describes. */
export async function createOfflineProvider(scriptPath: string): Promise<OfflineProvider> {
    const script = normalize(JSON.parse(await Bun.file(scriptPath).text()) as OfflineScript | OfflineStep[]);

    const handle = fauxProvider({
        provider: offlineProviderId,
        models: [{ id: offlineModelId, name: "Duet offline script" }],
        tokensPerSecond: script.tokensPerSecond ?? 0,
    });

    let seen: Context | undefined;

    handle.setResponses(
        script.steps.map(
            (step) => (context: Context, _options: SimpleStreamOptions | undefined, _state: FauxProviderState, _model: Model<string>) => {
                seen = context;

                return messageFor(step);
            },
        ),
    );

    return {
        handle,
        modelId: `${offlineProviderId}:${offlineModelId}`,
        locked: lockedTwin(handle),
        lockedModelId: `${lockedProviderId}:${lockedModelId}`,
        context: () => seen,
    };
}

/** The scripted provider again under another name, with auth that answers to
    nothing but a stored credential.

    It streams what its twin streams, because what it is for is the auth methods
    and not the loop: a run configured to it before a key is entered fails the
    way any unconfigured provider does, which is the plain message an invalid key
    is owed.
*/
function lockedTwin(handle: FauxProviderHandle): Provider {
    const model = { ...handle.models[0], id: lockedModelId, name: "Duet locked script", provider: lockedProviderId };

    return {
        ...handle.provider,
        id: lockedProviderId,
        name: "Duet locked script",
        getModels: () => [model],
        auth: {
            apiKey: {
                name: "Duet locked key",
                login: async (interaction: ProviderAuthInteraction): Promise<ApiKeyCredential> => ({
                    type: "api_key",
                    key: await interaction.prompt({ type: "secret", message: "Key?" }),
                }),
                resolve: async ({ credential }) =>
                    credential?.key === undefined || credential.key.length === 0
                        ? undefined
                        : { auth: { apiKey: credential.key }, source: "Duet locked key" },
            },
            oauth: {
                name: "Duet locked subscription",
                isSubscription: true,
                login: async (interaction: ProviderAuthInteraction): Promise<OAuthCredential> => {
                    interaction.notify({
                        type: "auth_url",
                        url: lockedAuthorizeUrl,
                        instructions: lockedInstructions,
                    });

                    const pasted = await interaction.prompt({
                        type: "manual_code",
                        message: "Paste the code the page ends on:",
                    });

                    if (pasted.trim() !== lockedCode) throw new Error("That code was not right. Try signing in again.");

                    return {
                        type: "oauth",
                        access: "offline-access",
                        refresh: "offline-refresh",
                        expires: Date.now() + 3600_000,
                    };
                },
                refresh: async (credential) => credential,
                toAuth: async () => ({ apiKey: "offline-access" }),
            },
        },
    };
}

function normalize(loaded: OfflineScript | OfflineStep[]): OfflineScript {
    return Array.isArray(loaded) ? { steps: loaded } : loaded;
}

function messageFor(step: OfflineStep) {
    // A step with no text of its own carries no content at all rather than an
    // empty string, which would stream as a commentary delta saying nothing.
    if (step.error !== undefined)
        return fauxAssistantMessage(step.text !== undefined ? step.text : [], {
            stopReason: "error",
            errorMessage: step.error,
        });

    const content: FauxContentBlock[] = [];

    if (step.text !== undefined) content.push(fauxText(step.text));

    if (step.toolCall !== undefined) content.push(fauxToolCall(step.toolCall.name, step.toolCall.arguments ?? {}));

    return fauxAssistantMessage(content, { stopReason: step.toolCall !== undefined ? "toolUse" : "stop" });
}
