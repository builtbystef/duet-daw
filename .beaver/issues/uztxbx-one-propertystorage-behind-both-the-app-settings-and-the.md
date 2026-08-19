---
id: uztxbx
title: One PropertyStorage behind both the app settings and the session engine
state: todo
priority: medium
labels:
    - maintenance
depends_on:
    - xxv9ng
created: 2026-08-19T20:34:00Z
updated: 2026-08-19T20:34:00Z
---

## What to build

App-global settings and a session's engine write the same file through two
different objects. `duet_app`'s `PropertyStorageSettings` constructs its own
`te::PropertyStorage`, and every `Session` constructs an `Engine` whose
PropertyStorage is the engine's default. Both resolve to one
`juce::PropertiesFile` path (`~/.config/Duet/Settings.xml`), and a
`PropertiesFile` writes the whole set it holds, from the snapshot it read when it
was made.

Duet's side already guards what it can: `setValue` reloads before it writes and
saves after, so it never drops a key the engine has put on disk. The engine's
side does no such thing, so a setting the producer changes while a project is
open can be overwritten when the engine next saves. Nothing has been seen to lose
a setting — the shape is what is wrong, and it gets worse as more app-global
settings land (window geometry, autosave interval, browser folders, favorites).

The fix is one store: the shell owns a single PropertyStorage and every Engine a
session makes is given it. `te::Engine`'s constructor takes ownership of a
`unique_ptr<PropertyStorage>`, so the one instance is reached through a
forwarding adapter rather than handed over. Whatever carries it across the model
facade must keep `duet_model`'s public interface free of engine types
(ADR-adjacent rule, `docs/ARCHITECTURE.md`).

## Acceptance criteria

- [ ] One PropertyStorage backs both `duet::gui::Settings` and every Engine a session makes; no second `juce::PropertiesFile` is opened on that path.
- [ ] `duet_model`'s public interface still names no engine or JUCE type — `duet_tests` links the facade and nothing else, and still compiles.
- [ ] A test shows a value written through `duet::gui::Settings` still readable after a session has opened and closed a project.
- [ ] `PropertyStorageSettings`'s reload-before-write comment is removed or restated: with one store it is no longer what makes the write safe.
