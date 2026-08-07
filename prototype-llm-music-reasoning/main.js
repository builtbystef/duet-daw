// PROTOTYPE — disposable. Electron main process embedding pi in-process.
import { app, BrowserWindow, ipcMain } from "electron";
import { mkdirSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
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
const RUNS_DIR = path.join(__dirname, "runs");
const EMPTY_AGENT_DIR = path.join(__dirname, ".empty-agent-dir");
const SYSTEM_PROMPT = readFileSync(path.join(__dirname, "SYSTEM_PROMPT.md"), "utf8");

const MODELS = [
  { key: "openai-codex/gpt-5.6-terra", provider: "openai-codex", id: "gpt-5.6-terra", label: "GPT-5.6 Terra" },
  { key: "xai/grok-4.5", provider: "xai", id: "grok-4.5", label: "Grok 4.5" },
];

let win = null;
let modelRuntime = null;
// The live session, kept so "continue" can prompt into the same conversation.
let current = null; // { session, fixtureId, modelKey, thinking, ctx }
let runCounter = 0;

function emit(payload) {
  if (win && !win.isDestroyed()) win.webContents.send("agent-event", payload);
}

function loadFixtures() {
  const fixtures = {};
  for (const file of readdirSync(FIXTURES_DIR).filter((f) => f.endsWith(".json")).sort()) {
    const data = JSON.parse(readFileSync(path.join(FIXTURES_DIR, file), "utf8"));
    fixtures[data.id] = data;
  }
  return fixtures;
}

// ---- The tool layer: the model perceives the fixture only through these. ----

function makeTools(fixture, ctx) {
  const track = (id) => fixture.tracks.find((t) => t.id === id);
  const trackIds = () => fixture.tracks.map((t) => t.id).join(", ");

  const wrap = (name, description, parameters, fn) =>
    defineTool({
      name,
      label: name,
      description,
      parameters,
      execute: async (_toolCallId, params) => {
        let result;
        try {
          result = fn(params ?? {});
        } catch (e) {
          result = { error: String(e?.message ?? e) };
        }
        emit({ runId: ctx.runId, type: "tool_call", tool: name, params: params ?? {}, result });
        ctx.trace.push({ tool: name, params: params ?? {}, result });
        return { content: [{ type: "text", text: JSON.stringify(result, null, 2) }], details: {} };
      },
    });

  const trackParam = Type.Object({
    trackId: Type.String({ description: "Track id from list_tracks" }),
  });

  return [
    wrap(
      "list_tracks",
      "List every track in the project: id, name, instrument, role, mixer state (volume, pan, mute, sends), routing, and what data exists per track (clips, MIDI patterns, plugins, automation).",
      Type.Object({}),
      () => ({
        projectName: fixture.meta.projectName,
        tracks: fixture.tracks.map((t) => ({
          id: t.id,
          name: t.name,
          instrument: t.instrument,
          role: t.role,
          routing: t.routing ?? "master",
          groupMembers: t.groupMembers ?? undefined,
          mixer: t.mixer,
          clipCount: t.clips.length,
          hasMidi: Object.keys(t.patterns).length > 0,
          pluginNames: t.pluginChain.map((p) => p.name),
          automatedParameters: t.automation.map((a) => a.parameter),
        })),
      }),
    ),
    wrap(
      "get_arrangement",
      "Project-level structure: key, tempo, time signature, bar count, the section list, and every track's clip placements on the timeline (clip name, start bar, length, which MIDI pattern it plays, whether it loops).",
      Type.Object({}),
      () => ({
        ...fixture.meta,
        sections: fixture.sections,
        trackClips: fixture.tracks
          .filter((t) => t.clips.length > 0)
          .map((t) => ({ trackId: t.id, clips: t.clips })),
      }),
    ),
    wrap(
      "get_track_analysis",
      "Deterministic DSP analysis of one track's rendered output: RMS, peak, integrated LUFS, crest factor, stereo width, and spectral energy in six bands (dBFS). Levels are measured post-fader.",
      trackParam,
      ({ trackId }) => {
        const t = track(trackId);
        if (!t) return { error: `Unknown track id '${trackId}'. Valid ids: ${trackIds()}` };
        return { trackId: t.id, name: t.name, analysis: t.analysis };
      },
    ),
    wrap(
      "get_midi",
      "The MIDI patterns of one track as raw note lists: beat offset within the pattern, length in beats, pitch (number and name), velocity. Audio-only tracks have no MIDI.",
      trackParam,
      ({ trackId }) => {
        const t = track(trackId);
        if (!t) return { error: `Unknown track id '${trackId}'. Valid ids: ${trackIds()}` };
        if (Object.keys(t.patterns).length === 0)
          return { trackId: t.id, name: t.name, error: "This track has no MIDI (audio clips or bus only)." };
        return { trackId: t.id, name: t.name, patterns: t.patterns };
      },
    ),
    wrap(
      "get_plugin_chain",
      "One track's plugin chain in order, with every parameter value, plus any automation envelopes on the track (parameter, breakpoints by bar).",
      trackParam,
      ({ trackId }) => {
        const t = track(trackId);
        if (!t) return { error: `Unknown track id '${trackId}'. Valid ids: ${trackIds()}` };
        return { trackId: t.id, name: t.name, pluginChain: t.pluginChain, automation: t.automation };
      },
    ),
  ];
}

// ---- Session construction: pi embedded the way OpenClaw does. ----

async function makeSession(fixture, modelDef, thinking, ctx) {
  const loader = new DefaultResourceLoader({
    cwd: __dirname,
    agentDir: EMPTY_AGENT_DIR,
    systemPromptOverride: () => SYSTEM_PROMPT,
    // Keep the experiment sealed: no skills, prompts, or AGENTS.md context
    // from this machine may leak into the model's context.
    skillsOverride: () => ({ skills: [], diagnostics: [] }),
    promptsOverride: () => ({ prompts: [], diagnostics: [] }),
    agentsFilesOverride: () => ({ agentsFiles: [] }),
  });
  await loader.reload();

  const model = modelRuntime.getModel(modelDef.provider, modelDef.id);
  if (!model) throw new Error(`Model not found: ${modelDef.key}`);

  const { session } = await createAgentSession({
    cwd: __dirname,
    agentDir: EMPTY_AGENT_DIR,
    model,
    thinkingLevel: thinking,
    modelRuntime,
    noTools: "builtin",
    customTools: makeTools(fixture, ctx),
    resourceLoader: loader,
    sessionManager: SessionManager.inMemory(),
    settingsManager: SettingsManager.inMemory({
      compaction: { enabled: false },
      retry: { enabled: true, maxRetries: 3 },
    }),
  });

  session.subscribe((event) => {
    if (event.type === "message_update") {
      const e = event.assistantMessageEvent;
      if (e.type === "text_delta") emit({ runId: ctx.runId, type: "text", delta: e.delta });
      if (e.type === "thinking_delta") emit({ runId: ctx.runId, type: "thinking", delta: e.delta });
    }
    if (event.type === "agent_end") emit({ runId: ctx.runId, type: "agent_end" });
  });

  return session;
}

function saveRun(run) {
  mkdirSync(RUNS_DIR, { recursive: true });
  const file = path.join(RUNS_DIR, `${run.startedAt.replace(/[:.]/g, "-")}-${run.fixtureId}-${run.modelId}.json`);
  writeFileSync(file, JSON.stringify(run, null, 2));
  return file;
}

async function singleRun({ fixtureId, modelKey, thinking, prompt }, { fresh }) {
  const fixtures = loadFixtures();
  const fixture = fixtures[fixtureId];
  const modelDef = MODELS.find((m) => m.key === modelKey);
  if (!fixture) throw new Error(`Unknown fixture: ${fixtureId}`);
  if (!modelDef) throw new Error(`Unknown model: ${modelKey}`);

  const runId = `run-${++runCounter}`;
  const reuse =
    !fresh &&
    current &&
    current.fixtureId === fixtureId &&
    current.modelKey === modelKey &&
    current.thinking === thinking;

  const ctx = reuse ? current.ctx : { runId, trace: [] };
  ctx.runId = runId;

  emit({
    runId,
    type: "run_start",
    fixtureId,
    model: modelDef.label,
    modelKey,
    thinking,
    prompt,
    continued: Boolean(reuse),
  });

  let session;
  if (reuse) {
    session = current.session;
  } else {
    current?.session?.dispose();
    session = await makeSession(fixture, modelDef, thinking, ctx);
    current = { session, fixtureId, modelKey, thinking, ctx };
  }

  const startedAt = new Date().toISOString();
  const traceStart = ctx.trace.length;
  try {
    await session.prompt(prompt);
  } catch (e) {
    emit({ runId, type: "run_error", error: String(e?.message ?? e) });
  }

  const run = {
    startedAt,
    finishedAt: new Date().toISOString(),
    fixtureId,
    modelKey,
    modelId: modelDef.id,
    thinking,
    prompt,
    continued: Boolean(reuse),
    toolCalls: ctx.trace.slice(traceStart).map(({ tool, params }) => ({ tool, params })),
    messages: session.messages,
  };
  const file = saveRun(run);
  emit({ runId, type: "run_saved", file: path.basename(file) });
}

// ---- IPC ----

ipcMain.handle("get-init", () => {
  const fixtures = loadFixtures();
  return {
    models: MODELS.map(({ key, label }) => ({ key, label })),
    fixtures: Object.values(fixtures).map((f) => ({
      id: f.id,
      projectName: f.meta.projectName,
      suggestedPrompt: f.suggestedPrompt,
      json: f,
    })),
    thinkingLevels: ["off", "minimal", "low", "medium", "high"],
  };
});

let busy = false;
ipcMain.handle("run", async (_e, opts) => {
  if (busy) return { error: "A run is already in progress." };
  busy = true;
  try {
    if (opts.mode === "both") {
      for (const m of MODELS) {
        await singleRun({ ...opts, modelKey: m.key }, { fresh: true });
      }
    } else {
      await singleRun(opts, { fresh: opts.mode !== "continue" });
    }
    return {};
  } catch (e) {
    return { error: String(e?.message ?? e) };
  } finally {
    busy = false;
  }
});

ipcMain.handle("abort", async () => {
  await current?.session?.abort();
  return {};
});

// ---- App lifecycle ----

app.whenReady().then(async () => {
  mkdirSync(EMPTY_AGENT_DIR, { recursive: true });
  mkdirSync(RUNS_DIR, { recursive: true });
  modelRuntime = await ModelRuntime.create(); // default ~/.pi/agent auth + models
  win = new BrowserWindow({
    width: 1400,
    height: 950,
    title: "PROTOTYPE — llm-music-reasoning (fod077)",
    webPreferences: { preload: path.join(__dirname, "preload.cjs") },
  });
  win.loadFile("index.html");
});

app.on("window-all-closed", () => app.quit());
