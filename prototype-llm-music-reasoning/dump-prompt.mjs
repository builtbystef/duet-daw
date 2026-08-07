// Throwaway: prints the exact system prompt + tool schemas the prototype sends.
import { readFileSync, readdirSync, mkdirSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { Type } from "typebox";
import {
  createAgentSession,
  DefaultResourceLoader,
  defineTool,
  ModelRuntime,
  SessionManager,
  SettingsManager,
} from "@earendil-works/pi-coding-agent";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const FIXTURES_DIR = path.join(__dirname, "fixtures");
const EMPTY_AGENT_DIR = path.join(__dirname, ".empty-agent-dir");
const SYSTEM_PROMPT = readFileSync(path.join(__dirname, "SYSTEM_PROMPT.md"), "utf8");

mkdirSync(EMPTY_AGENT_DIR, { recursive: true });

const fixtureFile = readdirSync(FIXTURES_DIR).filter((f) => f.endsWith(".json")).sort()[0];
const fixture = JSON.parse(readFileSync(path.join(FIXTURES_DIR, fixtureFile), "utf8"));

const trackParam = Type.Object({
  trackId: Type.String({ description: "Track id from list_tracks" }),
});

const wrap = (name, description, parameters) =>
  defineTool({ name, label: name, description, parameters, execute: async () => ({ content: [], details: {} }) });

const tools = [
  wrap(
    "list_tracks",
    "List every track in the project: id, name, instrument, role, mixer state (volume, pan, mute, sends), routing, and what data exists per track (clips, MIDI patterns, plugins, automation).",
    Type.Object({}),
  ),
  wrap(
    "get_arrangement",
    "Project-level structure: key, tempo, time signature, bar count, the section list, and every track's clip placements on the timeline (clip name, start bar, length, which MIDI pattern it plays, whether it loops).",
    Type.Object({}),
  ),
  wrap(
    "get_track_analysis",
    "Deterministic DSP analysis of one track's rendered output: RMS, peak, integrated LUFS, crest factor, stereo width, and spectral energy in six bands (dBFS). Levels are measured post-fader.",
    trackParam,
  ),
  wrap(
    "get_midi",
    "The MIDI patterns of one track as raw note lists: beat offset within the pattern, length in beats, pitch (number and name), velocity. Audio-only tracks have no MIDI.",
    trackParam,
  ),
  wrap(
    "get_plugin_chain",
    "One track's plugin chain in order, with every parameter value, plus any automation envelopes on the track (parameter, breakpoints by bar).",
    trackParam,
  ),
];

const loader = new DefaultResourceLoader({
  cwd: __dirname,
  agentDir: EMPTY_AGENT_DIR,
  systemPromptOverride: () => SYSTEM_PROMPT,
  skillsOverride: () => ({ skills: [], diagnostics: [] }),
  promptsOverride: () => ({ prompts: [], diagnostics: [] }),
  agentsFilesOverride: () => ({ agentsFiles: [] }),
});
await loader.reload();

const modelRuntime = await ModelRuntime.create();
const model = modelRuntime.getModel("openai-codex", "gpt-5.6-terra");
if (!model) throw new Error("Model not found");

const { session } = await createAgentSession({
  cwd: __dirname,
  agentDir: EMPTY_AGENT_DIR,
  model,
  thinkingLevel: "medium",
  modelRuntime,
  noTools: "builtin",
  customTools: tools,
  resourceLoader: loader,
  sessionManager: SessionManager.inMemory(),
  settingsManager: SettingsManager.inMemory({
    compaction: { enabled: false },
    retry: { enabled: true, maxRetries: 3 },
  }),
});

console.log("================ SYSTEM PROMPT (as sent) ================\n");
console.log(session.systemPrompt);
console.log("\n================ TOOL SCHEMAS (as sent) ================\n");
for (const t of tools) {
  console.log(JSON.stringify({ name: t.name, description: t.description, parameters: t.parameters }, null, 2));
  console.log();
}
console.log(`(fixture used for tool construction: ${fixture.id})`);
process.exit(0);
