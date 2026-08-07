// PROTOTYPE — disposable. Token-free smoke test: fixtures parse and are
// internally consistent, both models resolve with auth, and a pi session
// constructs in-process with the custom tools registered.
import { mkdirSync, readdirSync, readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  createAgentSession,
  DefaultResourceLoader,
  ModelRuntime,
  SessionManager,
  SettingsManager,
  defineTool,
} from "@earendil-works/pi-coding-agent";
import { Type } from "typebox";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const FIXTURES_DIR = path.join(__dirname, "fixtures");
let failures = 0;
const fail = (msg) => { failures++; console.error("  FAIL:", msg); };

// 1. Fixtures
const fixtures = [];
for (const file of readdirSync(FIXTURES_DIR).filter((f) => f.endsWith(".json")).sort()) {
  const fx = JSON.parse(readFileSync(path.join(FIXTURES_DIR, file), "utf8"));
  fixtures.push(fx);
  const ids = new Set();
  for (const t of fx.tracks) {
    if (ids.has(t.id)) fail(`${fx.id}: duplicate track id ${t.id}`);
    ids.add(t.id);
    for (const c of t.clips) {
      if (c.pattern && !t.patterns[c.pattern]) fail(`${fx.id}/${t.id}: clip references missing pattern '${c.pattern}'`);
      if (c.startBar + c.lengthBars - 1 > fx.meta.barCount) fail(`${fx.id}/${t.id}: clip '${c.name}' exceeds barCount`);
    }
    for (const [name, p] of Object.entries(t.patterns)) {
      for (const n of p.notes) {
        if (n.beat + n.lengthBeats > p.lengthBars * 4 + 0.001) fail(`${fx.id}/${t.id}/${name}: note at beat ${n.beat} exceeds pattern length`);
        if (n.pitch < 0 || n.pitch > 127 || n.velocity < 1 || n.velocity > 127) fail(`${fx.id}/${t.id}/${name}: note out of MIDI range`);
      }
    }
    if (!t.analysis?.spectralBandsDb) fail(`${fx.id}/${t.id}: missing spectral analysis`);
  }
  const sectionEnd = Math.max(...fx.sections.map((s) => s.startBar + s.lengthBars - 1));
  if (sectionEnd !== fx.meta.barCount) fail(`${fx.id}: sections end at ${sectionEnd}, barCount is ${fx.meta.barCount}`);
  if (!fx.suggestedPrompt) fail(`${fx.id}: missing suggestedPrompt`);
}
console.log(`fixtures: ${fixtures.length} loaded (${fixtures.map((f) => f.id).join(", ")})`);

// 2. Models + auth
const modelRuntime = await ModelRuntime.create();
for (const [provider, id] of [["openai-codex", "gpt-5.6-terra"], ["xai", "grok-4.5"]]) {
  const model = modelRuntime.getModel(provider, id);
  if (!model) { fail(`model not found: ${provider}/${id}`); continue; }
  const status = await modelRuntime.checkAuth(provider);
  console.log(`model: ${provider}/${id} resolves, auth: ${JSON.stringify(status)}`);
}

// 3. Session constructs in-process with custom tools, no built-ins
const EMPTY_AGENT_DIR = path.join(__dirname, ".empty-agent-dir");
mkdirSync(EMPTY_AGENT_DIR, { recursive: true });
const loader = new DefaultResourceLoader({
  cwd: __dirname,
  agentDir: EMPTY_AGENT_DIR,
  systemPromptOverride: () => "smoke test",
  skillsOverride: () => ({ skills: [], diagnostics: [] }),
  promptsOverride: () => ({ prompts: [], diagnostics: [] }),
  agentsFilesOverride: () => ({ agentsFiles: [] }),
});
await loader.reload();
const dummy = defineTool({
  name: "list_tracks", label: "list_tracks", description: "d",
  parameters: Type.Object({}),
  execute: async () => ({ content: [{ type: "text", text: "{}" }], details: {} }),
});
const { session } = await createAgentSession({
  cwd: __dirname,
  agentDir: EMPTY_AGENT_DIR,
  model: modelRuntime.getModel("xai", "grok-4.5"),
  modelRuntime,
  noTools: "builtin",
  customTools: [dummy],
  resourceLoader: loader,
  sessionManager: SessionManager.inMemory(),
  settingsManager: SettingsManager.inMemory({ compaction: { enabled: false } }),
});
const toolNames = session.agent.state.tools.map((t) => t.name);
console.log(`session: constructed, tools = [${toolNames.join(", ")}]`);
if (toolNames.some((n) => ["read", "bash", "edit", "write"].includes(n))) fail("built-in coding tools leaked into the session");
if (!toolNames.includes("list_tracks")) fail("custom tool missing from session");
session.dispose();

console.log(failures ? `\n${failures} failure(s)` : "\nsmoke test OK");
process.exit(failures ? 1 : 0);
