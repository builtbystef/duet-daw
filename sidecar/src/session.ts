// The agent session: pi's loop, wearing Duet's prompt and Duet's tools, with
// every tool call forwarded across the socket to the DAW.
//
// One session lives as long as the sidecar does, and the transcript is the
// conversation the producer sees in the panel — each Task Run is another turn of
// it, not a fresh start, which is both what makes it a conversation and what
// gives a provider's prompt cache a prefix worth keeping.
//
// A run is fire-and-forget by design: `startRun` hands the prompt to the loop and
// returns, and everything the DAW learns about the run afterwards arrives as one
// of the three notifications. That is the same shape the DAW side has (xy9438) —
// nobody waits on anybody.

import { Agent, type AgentTool } from "@earendil-works/pi-agent-core";
import type { Models } from "@earendil-works/pi-ai";

import { assemblePrompt } from "./prompt.ts";
import { vocabulary } from "./vocabulary.ts";

/** Where a run's commentary, tool activity and ending go. */
export interface RunEvents {
    text(runId: string, delta: string): void;
    toolActivity(runId: string, tool: string, phase: "start" | "end"): void;
    finished(runId: string, status: "completed" | "canceled" | "failed", error?: string): void;
}

/** What the DAW says was true of the producer when a run began. */
export interface OpeningContext {
    selection?: { kind?: string; ids?: string[] };
    playhead?: { bar?: number; beat?: number };
    transportPlaying?: boolean;
}

/** Answers one tool call, by asking the DAW. Rejects when the DAW refuses. */
export type ToolCaller = (call: {
    runId: string;
    callId: string;
    tool: string;
    args: Record<string, unknown>;
}) => Promise<unknown>;

export class CollaboratorSession {
    private agent: Agent | undefined;
    private runId: string | undefined;
    private canceled = false;
    private systemPrompt = "";

    constructor(
        private readonly callTool: ToolCaller,
        private readonly events: RunEvents,
    ) {}

    /** The model and the prompt parameters, from the DAW's `configure`.

        Building the agent here rather than at each run is what keeps the system
        prompt one frozen string for the life of a configuration.
    */
    configure(models: Models, modelSelection: string, promptParameters: Record<string, unknown>): void {
        const separator = modelSelection.indexOf(":");

        if (separator <= 0) throw new Error(`a model is written provider:id, and ${modelSelection} is not`);

        const provider = modelSelection.slice(0, separator);
        const modelId = modelSelection.slice(separator + 1);
        const model = models.getModel(provider, modelId);

        if (model === undefined) throw new Error(`no provider offers the model ${modelSelection}`);

        this.systemPrompt = assemblePrompt(promptParameters);
        this.agent = new Agent({
            initialState: { model, systemPrompt: this.systemPrompt, tools: this.tools() },
            streamFn: models.streamSimple.bind(models),
        });

        this.agent.subscribe((event) => {
            const run = this.runId;

            if (run === undefined) return;

            if (event.type === "message_update" && event.assistantMessageEvent.type === "text_delta")
                this.events.text(run, event.assistantMessageEvent.delta);
            else if (event.type === "tool_execution_start") this.events.toolActivity(run, event.toolName, "start");
            else if (event.type === "tool_execution_end") this.events.toolActivity(run, event.toolName, "end");
        });
    }

    /** The prompt the agent is configured with, for `--dump-prompt`. */
    get prompt(): string {
        return this.systemPrompt;
    }

    get activeRunId(): string | undefined {
        return this.runId;
    }

    /** Begins a run, and returns at once. The ending arrives as an event.

        Only ever called on a configured session: without a `configure` there is
        no session at all, and the host answers for that case itself.
    */
    startRun(runId: string, prompt: string, context: OpeningContext | undefined): void {
        if (this.runId !== undefined) throw new Error(`run ${this.runId} is still going`);

        const agent = this.agent;

        if (agent === undefined) throw new Error("this session has no model");

        this.runId = runId;
        this.canceled = false;

        void agent
            .prompt(describe(context) + prompt)
            .then(() => {
                if (this.canceled) return this.end(runId, "canceled");

                const failure = agent.state.errorMessage;

                return failure === undefined || failure.length === 0
                    ? this.end(runId, "completed")
                    : this.end(runId, "failed", failure);
            })
            .catch((thrown: unknown) => {
                this.end(runId, "failed", thrown instanceof Error ? thrown.message : String(thrown));
            });
    }

    /** Stops the provider request in flight. The run ends canceled. */
    cancel(runId: string): void {
        if (this.runId !== runId) return;

        this.canceled = true;
        this.agent?.abort();
    }

    /** Ends the run once, and never twice. */
    private end(runId: string, status: "completed" | "canceled" | "failed", error?: string): void {
        if (this.runId !== runId) return;

        this.runId = undefined;
        this.events.finished(runId, status, error);
    }

    /** The Tool Vocabulary, each declaration given the one body it has: ask the
        DAW, and hand back what it says.

        A refusal is thrown rather than returned. pi's loop turns a throw into an
        error tool result the model sees and can correct against, which is what
        the spec asks of an unknown id or an out-of-range value in a `suggest`
        call, and of a tool this DAW does not answer yet.
    */
    private tools(): AgentTool[] {
        return vocabulary.map((declared) => ({
            name: declared.name,
            label: declared.label,
            description: declared.description,
            parameters: declared.parameters,
            execute: async (callId: string, args: unknown) => {
                const runId = this.runId;

                if (runId === undefined) throw new Error("there is no run to answer for");

                const result = await this.callTool({
                    runId,
                    callId,
                    tool: declared.name,
                    args: (args as Record<string, unknown> | undefined) ?? {},
                });

                return { content: [{ type: "text" as const, text: JSON.stringify(result ?? {}) }], details: result };
            },
        }));
    }
}

/** The opening context as a line the model reads before the producer's words.

    It is a stage direction, not a tool result: it says what was true the moment
    the producer pressed send, and it is true nowhere else (u24m3x), so it is
    stated once here rather than made askable.
*/
function describe(context: OpeningContext | undefined): string {
    if (context === undefined) return "";

    const parts: string[] = [];
    const selection = context.selection;

    if (selection?.kind === "clips" || selection?.kind === "tracks") {
        const ids = selection.ids ?? [];

        parts.push(`${selection.kind} selected: ${ids.length > 0 ? ids.join(", ") : "none named"}`);
    } else {
        parts.push("nothing selected");
    }

    const playhead = context.playhead;

    if (playhead !== undefined) parts.push(`playhead at bar ${playhead.bar ?? 1}, beat ${playhead.beat ?? 1}`);

    parts.push(context.transportPlaying === true ? "transport playing" : "transport stopped");

    return `[As they asked: ${parts.join("; ")}.]\n\n`;
}
