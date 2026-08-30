---
id: zxpgna
title: A plugin that raises when read still reaches the message loop everywhere but the read tools
state: todo
priority: medium
labels:
    - bug
created: 2026-08-30T19:07:09Z
updated: 2026-08-30T19:07:09Z
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
