---
id: otk1nr
title: Graphite parameter windows for 4OSC, EQ, compressor, and reverb
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
depends_on:
    - 8ah7je
parent: jt5rjt
created: 2026-09-01T18:35:37Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Make `PluginEditorManager` open Duet's custom editor for the four parameterized built-ins while retaining the native/generic path for external VST3s.

## Settled editor shape

- One floating window per plugin ref; reopening focuses it. The manager closes it on plugin deletion, project replacement, or shutdown.
- Chrome is Graphite and contains device name, Bypass, preset selector/save, and Close. A scrollable labelled control is generated for every parameter snapshot in vocabulary order; sliders use range/skew, display the model string and unit, and named 0/1 values such as Reverb Freeze use a toggle.
- The editor never requests a built-in processor through ADR 0008. Components bind only to `BuiltinParameterEditor`; `PluginEditorAccess::processorOf` remains external-plugin-only.
- Keyboard: Tab follows visual order, arrows adjust one percent of range (Shift one tenth), Home/End choose bounds, Enter starts numeric entry, Escape cancels the active gesture before closing anything.

## Acceptance and tests

- [ ] All four devices open, focus existing windows, expose every vocabulary parameter exactly once, and close safely.
- [ ] Mouse, keyboard, automation refresh, Bypass, cancel, and Action naming use the shared model behavior.
- [ ] Both themes/scales use project tokens with no default generic JUCE editor inside a built-in window.
- [ ] Component tests assert composition, focus, lifetime, and gesture routing; they do not assert pixels.

Start in `PluginEditorManager.h/.cpp`, add focused reusable components under `duet_gui_components`, and extend `tests/gui`. Run all AGENTS.md checks before closing.
