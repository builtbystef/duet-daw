---
id: uztxbx
title: One PropertyStorage behind both the app settings and the session engine
state: done
assignee: claude
priority: medium
labels:
    - maintenance
depends_on:
    - xxv9ng
created: 2026-08-19T20:34:00Z
updated: 2026-08-26T12:59:28Z
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

## Notes

**claude** — 2026-08-26T12:59:19Z

Built (2026-08-26). All four checks pass: format clean, lint sweep clean, full build, 317/317 ctest.

## What landed

**One store, owned by the model.** `DuetPropertyStorage` — the one
`juce::PropertiesFile` on `Settings.xml` — is now reached through a
`juce::SharedResourcePointer`, so it is made on first use and written out when
the last holder goes. Two kinds of holder exist. `duet::model::AppSettings` is
the engine-free handle (string in, string out, `std::optional` out for a key
nobody has set); `SharedPropertyStorage` is the forwarding adapter each session's
Engine is given, because `te::Engine`'s constructor takes ownership of a
`unique_ptr<PropertyStorage>` and the shared instance cannot be handed over.

**Which way the crossing goes.** The model owns the store rather than the shell,
because `PropertyStorage` is the engine's own class and the model is the module
that may name engine types. The shell's `PropertyStorageSettings` is now a
`duet::gui::Settings` over `duet::model::AppSettings` and names no engine type at
all; the header carrying it moved to `duet/app/PropertyStorageSettings.h` and the
source into `duet_app_core`, so `duet_tests` links the store the shell runs on
instead of a stand-in. The version stamp moved with it, to `Main.cpp`, where
`JUCE_APPLICATION_VERSION_STRING` is defined and where the version is the
application's own.

**The adapter forwards every virtual**, the ones the base would answer through
`getPropertiesFile()` included: what the engine reads or writes has to reach the
one file, and that must not rest on which of the base's methods happen to route
through a virtual today.

## The test, and what it pins

`tests/AppSettingsTests.cpp`: a session opens, the engine writes an app-global
setting of its own (`suppressDeviceRebuild`), the producer changes a setting
through `duet::gui::Settings` while the project is open, and the project closes.
The next launch reads what is on disk. Verified red for the right reason — with
`std::make_unique<DuetPropertyStorage>()` put back on the session's Engine, so
that both sides resolve the same path with a store each, the value is gone and
the test fails. Restored, it passes.

A second effect of the one store, worth knowing: the app's side used to
construct a bare `te::PropertyStorage`, whose prefs folder comes from JUCE's
cached special locations — which a test process cannot redirect, because the
cache is filled before `main`. So the shell's settings in `duet_tests` used to
resolve to the producer's real `~/.config/Duet/Settings.xml`. They now resolve
through `DuetPropertyStorage`, which reads XDG_CONFIG_HOME itself, so the suite
stays inside its isolated settings home.

## Decisions a reviewer should know

- **Reload is gone, the flush stays.** `setValue` no longer re-reads the file
  before writing; with one store there is no second set to merge, and the reload
  would in fact have dropped whatever the engine had set but not yet saved. The
  save after each write stays, for the reason it always had: a setting the
  producer changes is on disk before the next thing that can end the process.
  `PropertyStorageSettings`'s comment says that and nothing about merging.
- **Lifetime by reference count, not by a singleton.** The store lives while a
  holder does. In the app that is the whole run — the shell holds one from
  `initialise` to `shutdown` — and a session's Engine holds another for as long
  as the project is open. Nothing is destroyed after JUCE's own shutdown.
- **Recorded in docs/ENGINE_NOTES.md** (further facts): a `PropertyStorage` is a
  whole `Settings.xml`, held from the moment it is read.

## Not done here

The shell was not run. This session had no writable path to the producer's real
`~/.config/Duet/Settings.xml` and no reason to open the producer's last project,
so the evidence is the suite's, not a screenshot's. The producer-visible path is
unchanged: with XDG_CONFIG_HOME set or unset, the store resolves to
`~/.config/Duet/Settings.xml` exactly as before.
