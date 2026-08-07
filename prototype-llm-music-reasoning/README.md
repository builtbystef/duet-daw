# PROTOTYPE — llm-music-reasoning (disposable, do not ship)

Answers Beaver issue `fod077`: **given structured, tool-produced music data and no
audio, does an LLM say anything a producer would actually act on?**

A throwaway Electron app that embeds pi in-process (`createAgentSession`, built-in
tools disabled, everything served through `customTools`). The model perceives a
fixture project only by calling tools — `list_tracks`, `get_arrangement`,
`get_track_analysis`, `get_midi`, `get_plugin_chain` — and the UI shows the
streamed response plus the tool-call trace. The trace is half the finding.

## Run

```
npm install
npm start
```

Requires the globally configured pi auth (`~/.pi/agent/auth.json`) with
`openai-codex` and `xai` OAuth — already present on this machine.

`npm run smoke` checks fixtures, tools, and model resolution without spending tokens.

## Layout

- `fixtures/*.json` — seven hand-written project fixtures. Only data a
  deterministic analysis tool could produce: project meta, sections, clip
  placement, MIDI note lists, mixer state, plugin chains with parameters,
  per-track level/spectral analysis. **Track and project names must not leak
  the diagnosis.**
- `fixtures/GROUND_TRUTH.md` — what is actually wrong in each fixture and the
  fix the producer accepted. NEVER served to the model. Draft — to be corrected
  against real sessions.
- `SYSTEM_PROMPT.md` — the Collaborator system prompt (part of the experiment
  surface; edit and re-run).
- `runs/` — every completed run saved as JSON (full messages + tool trace).
  These transcripts are the raw material of the deliverable note.

## What this does not prove

Whether pi can be embedded in the real C++/JUCE DuetDAW (open at node
`hcxgfv`). Electron is the environment where embedding is trivially free.
