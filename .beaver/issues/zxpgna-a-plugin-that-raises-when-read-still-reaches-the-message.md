---
id: zxpgna
title: A plugin that raises when read still reaches the message loop everywhere but the read tools
state: done
assignee: claude
priority: medium
labels:
    - bug
created: 2026-08-30T19:07:09Z
updated: 2026-08-30T20:06:08Z
---

## What is wrong

A hosted plugin is free to raise when it is asked what one of its values means,
and the exception unwinds out of `Session::pluginParameters` on whatever thread
asked (ENGINE_NOTES.md, proved by `tests/vst3_fixtures/RaisingPlugin.cpp`).

qf9e9h confined that to the plugin it came from for the two tools that read a
chain, and `ProjectTools::read` is the floor under the rest of a Tool Vocabulary
read. Neither covers the other places Duet asks a plugin the same question, and
every one of them runs on the message thread with nothing to catch:

- **The `suggest` path.** `SuggestTool`'s `Project::parameter` reads a plugin's
  parameters to check the id and the range of a `plugin.setParam` operation, and
  the marshalled read it runs inside has no `try` at all — unlike
  `ProjectTools::read`, which grew one with 97ynt7. So a Suggestion naming a
  parameter of a hostile plugin is an exception in the DAW's own message loop
  rather than a validation error the model can correct.
- **Staleness.** `SuggestionManager`'s `describeParameters` reads the parameters
  of every plugin a pending Suggestion names, to say what the project said about
  it. A producer edit measures every pending Suggestion for staleness.
- **The interface.** `AutomationLanes.cpp` reads a plugin's parameters to build
  the lane list, and `PluginEditorManager.cpp` reads them around a gesture in a
  native editor. A producer who never asks the Collaborator anything reaches
  these by opening a menu.

Read from the sources, not measured: the raising fixture has only ever been
pointed at the read tools. A first step is to point it at each of these.

## What to decide, then build

Whether the guard belongs at each caller or at the one place they all go
through. `Session::pluginParameters` answering "the plugin would not say" for
every caller at once is the smaller change and the larger contract move — it
takes a real failure and gives it the same shape as a plugin with no parameters
unless the model layer grows a way to tell the two apart, which `get_plugin_chain`
now needs (`parametersReadable`).

## Acceptance criteria

- [ ] A `suggest` naming a parameter of a plugin that raises is answered with a
      validation error the run survives; nothing reaches the message loop.
- [ ] A producer edit that measures a pending Suggestion naming such a plugin
      for staleness leaves the DAW running.
- [ ] The automation lane list and a native editor gesture on such a plugin
      leave the DAW running.
- [ ] Each of those is asserted with the raising VST3 fixture, the way qf9e9h
      asserts the read tools.

## Notes

**claude** — 2026-08-30T20:06:08Z

## The decision

The guard belongs at the one place they all go through, and that place is the
model facade rather than each caller. `duet_model` is the engine seam and its
public interface exposes zero engine types (ARCHITECTURE.md); an exception
thrown inside a hosted plugin is an engine type, so stopping it at the facade is
the rule that already exists rather than a new one. Every caller named in the
issue — `SuggestTool`'s `Project::parameter`, `SuggestionManager`'s
`describeParameters`, `AutomationLanes::targetsFor`, `PluginEditorManager`'s
four gesture reads — reads through `Session::pluginParameters`, and all of them
run on the message thread.

The issue's objection to that — it gives a real failure the same shape as a
plugin with no parameters — is answered by the model growing the way to tell
them apart, which is what the issue said it would take. `PluginParameterRead`
carries `held` and `wereRead`; `Session::readPluginParameters` is the guarded
read, and `Session::pluginParameters` is that read without the refusal, so
every caller that only wants the parameters is unchanged and safe.

## What was built

- `Session::readPluginParameters` catches everything (not just what derives from
  `std::exception`: the type a plugin throws is the plugin's to choose) and
  answers `wereRead: false` with an empty list. `pluginParameters` is
  `readPluginParameters(...).held`. A ref that names no plugin is neither state:
  nothing was asked, so `wereRead` stays true — which is what it read back as
  before, so no caller changed behaviour there.
- `ProjectTools` dropped its own local `PluginParameters` struct and catch from
  `qf9e9h` and delegates to the model. `get_plugin_chain` still says
  `parametersReadable`, now off `readPluginParameters`; `ProjectTools::read`
  keeps its own catch as the floor under everything else.
- `suggest` refuses an operation on such a plugin for the plugin's refusal
  rather than for the parameter's name: `hasParameter` became
  `parameterProblem`, which answers "this plugin would not say what parameters
  it has". Telling the Collaborator the name was wrong would send it hunting for
  one that was right all along, which is a validation error it cannot correct.
- `tests/Vst3FixtureHarness.h` — the copy, the scan, the two fixture names and
  `raiseWhenRead`, which four suites now share. `ProjectToolsTests` and
  `PluginHostingTests` dropped their own copies of the first two.

## Seams

- **`suggest`** — `SuggestToolTests`, through a `ToolRun`: a run that survives,
  a result that is empty, and the message that names the refusal.
- **Staleness** — `SuggestionManagerTests`, through the manager's own interface
  with the real write-tool under it: `REQUIRE_NOTHROW` on the producer edit that
  measures every pending Suggestion, and the Suggestion is stale afterwards
  because what the project can say about the plugin changed.
- **The lane list** — `AutomationLaneTests`, through `AutomationLanes::targetsFor`.
- **A native editor gesture** — asserted at the model seam in
  `PluginHostingTests`, not at `PluginEditorManager`. Its four reads are
  `session->pluginParameters(plugin)` and nothing else, and it lives in the
  JUCE-linked half where no suite opens a window: driving a real
  `DocumentWindow` and a mouse gesture to observe one read of the facade would
  be a bigger and more fragile test than the guarantee is. The model-seam case
  names the callers it stands for.

## Facts a reviewer needs

- `SuggestionManager`'s staleness descriptor for a hostile plugin reads
  `params []`. It was left that way deliberately: descriptors are baselines that
  are only ever compared with themselves, never shown to the model, and the text
  already changes when a plugin stops answering, so the Suggestion goes stale on
  its own. Nothing is gained by writing the refusal into it.
- `builtinPluginParameters` is not guarded. The built-ins are Duet's own and the
  engine's; the raise this issue is about is a hosted plugin's.
- Checks: format clean, `./scripts/lint.sh` clean, 612/612 ctest.
