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
    type Context,
    type Model,
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

    /** The context of the most recent request: the system prompt and the tool
        list exactly as the provider received them, and the transcript as it
        stood — which by the end of a run holds the tool results the DAW sent
        back, so a test can see that they reached the model.
    */
    context(): Context | undefined;
}

export const offlineProviderId = "duet-offline";
export const offlineModelId = "scripted";

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

    return { handle, modelId: `${offlineProviderId}:${offlineModelId}`, context: () => seen };
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
