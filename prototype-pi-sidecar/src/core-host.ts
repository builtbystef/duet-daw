// PROTOTYPE — disposable measurement code. Do not ship.
import { readFileSync } from "node:fs";
import { homedir } from "node:os";
import { join } from "node:path";

import { Agent, type AgentTool } from "@earendil-works/pi-agent-core";
import {
  InMemoryCredentialStore,
  Type,
  createModels,
  type Credential,
} from "@earendil-works/pi-ai";
import { registerBunOAuthFlows } from "@earendil-works/pi-ai/bun-oauth";
import { openaiCodexProvider } from "@earendil-works/pi-ai/providers/openai-codex";
import { xaiProvider } from "@earendil-works/pi-ai/providers/xai";

const argv = new Set(process.argv.slice(2));
const startedAt = performance.now();
registerBunOAuthFlows();

const inspectTool: AgentTool = {
  name: "inspect_project",
  label: "Inspect project",
  description: "Return the name of the current disposable prototype project.",
  parameters: Type.Object({}),
  async execute() {
    return {
      content: [{ type: "text", text: "Project name: Duet prototype" }],
      details: {},
    };
  },
};

function dump(label: string, value: unknown) {
  process.stdout.write(`${JSON.stringify({ label, value })}\n`);
}

const requestShape = {
  systemPrompt: "",
  messages: [],
  tools: [
    {
      name: inspectTool.name,
      description: inspectTool.description,
      parameters: inspectTool.parameters,
    },
  ],
};

dump("assembled-agent-request", requestShape);

if (argv.has("--inspect")) {
  dump("runtime", { bun: typeof Bun !== "undefined", node: process.version });
  process.exit(0);
}

const credentials = new InMemoryCredentialStore();
const authPath = join(homedir(), ".pi", "agent", "auth.json");
const stored = JSON.parse(readFileSync(authPath, "utf8")) as Record<string, Credential>;
for (const [provider, credential] of Object.entries(stored)) {
  await credentials.modify(provider, async () => credential);
}

const models = createModels({ credentials });
models.setProvider(xaiProvider());
models.setProvider(openaiCodexProvider());
const selection = process.env.PROTOTYPE_MODEL ?? "openai-codex:gpt-5.6-luna";
const separator = selection.indexOf(":");
const provider = selection.slice(0, separator);
const modelId = selection.slice(separator + 1);
const model = models.getModel(provider, modelId);
if (!model) throw new Error(`Unknown model: ${selection}`);

let firstProviderEventAt: number | undefined;
let firstTextAt: number | undefined;
let capturedProviderPayload = false;
const agent = new Agent({
  initialState: { model, systemPrompt: "", tools: [inspectTool] },
  streamFn: models.streamSimple.bind(models),
  onPayload(payload) {
    if (!capturedProviderPayload) {
      capturedProviderPayload = true;
      const body = payload as Record<string, unknown>;
      dump("assembled-provider-request", {
        model: body.model,
        instructions: body.instructions,
        tools: body.tools,
      });
    }
  },
});

agent.subscribe((event) => {
  if (event.type === "message_update") {
    const update = event.assistantMessageEvent;
    if (
      firstProviderEventAt === undefined &&
      (update.type === "text_delta" || update.type === "toolcall_delta")
    ) {
      firstProviderEventAt = performance.now();
      dump("first-provider-event", {
        milliseconds: firstProviderEventAt - startedAt,
        type: update.type,
      });
    }
    if (update.type === "text_delta") {
      if (firstTextAt === undefined) {
        firstTextAt = performance.now();
        dump("first-text", { milliseconds: firstTextAt - startedAt });
      }
      process.stdout.write(update.delta);
    } else if (update.type === "toolcall_end") {
      dump("tool-call", update.toolCall);
    }
  } else if (event.type === "tool_execution_end") {
    dump("tool-result", { name: event.toolName, result: event.result });
  }
});

await agent.prompt(
  "Call inspect_project exactly once. After it returns, reply with the project name only.",
);
if (agent.state.errorMessage) throw new Error(agent.state.errorMessage);
process.stdout.write("\n");
