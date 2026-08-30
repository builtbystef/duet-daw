---
id: 4v2m38
title: duet_gui_components compiles JUCE's plugin host differently from duet_model
state: todo
priority: medium
labels:
    - bug
created: 2026-08-29T12:13:01Z
updated: 2026-08-29T12:13:01Z
---

## What was seen

`duet::gui_components` links `juce::juce_audio_utils` through `duet_juce_config`,
which does not carry `JUCE_PLUGINHOST_VST3=1`; `duet::model` compiles the same
JUCE modules through `duet_engine_config`, which does. JUCE modules are INTERFACE
sources — they compile into each consuming target — so the two targets hold two
differently configured copies of `juce_audio_processors`, and which one an
executable ends up calling is the linker's choice.

What it looks like from outside: in `duet_gui_tests`, which links
`duet::gui_components`, `duet::model::Session::canHostVst3()` answers false, while
the same call in `duet_tests` answers true. Found while writing the plugin-scan
dialog's component tests (issue zm174o), which had to drop a real scan and assert
the empty-directory path instead; the scan itself is asserted in the paintless
suite, so nothing is untested — but the component suite cannot reach a VST3 host.

The root CMakeLists says this exactly: "JUCE modules are INTERFACE sources: they
compile into each consuming target, and a definition that differs between two
targets would be an ODR violation rather than a build error."

## Why it matters

It is an ODR violation in the shipping application, not only in the tests:
`duet_app` links both targets. Today the symptom is confined to which copy of the
format manager answers, and the app happens to work because the engine's own
`PluginManager` is compiled in `duet_model`. It is the kind of thing that changes
behaviour when a link order changes.

## What to do

Decide where the plugin-host definitions belong and give every target that
compiles `juce_audio_processors` the same ones — most likely by having
`duet_gui_components` link `duet_engine_config` rather than `duet_juce_config`,
or by moving the plugin-host definitions into `duet_juce_config`. Then assert it:
a case in the component suite that `canHostVst3()` agrees with the paintless
suite's answer is what keeps the two configurations one.
