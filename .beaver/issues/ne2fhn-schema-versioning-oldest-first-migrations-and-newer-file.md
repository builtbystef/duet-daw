---
id: ne2fhn
title: 'Schema versioning: oldest-first migrations and newer-file refusal'
state: done
assignee: agent
priority: medium
depends_on:
    - 1c8sjh
parent: b1j3me
created: 2026-08-11T01:51:05Z
updated: 2026-08-24T06:51:40Z
---

## What to build

Version discipline for the DUET tree per spec b1j3me: every project file carries an integer schema version; on load, migrations run oldest-first, one step per version, before anything reads the tree; a file saved by a newer Duet is refused with a clear message naming the needed version rather than silently damaged. The running application's version is stamped into the engine's property storage.

## Acceptance criteria

- [ ] Every saved project carries the current integer schema version in its DUET tree.
- [ ] Worked example (current version N): a file stamped N+1 is refused on open — the project stays untouched on disk and the message names version N+1 as requiring a newer Duet.
- [ ] Worked example: a file stamped N−1 runs its migration step, then opens normally; a file stamped N−2 runs both steps in order, oldest first, each step raising the version by exactly one. (Ship at least one real or scaffold migration so the chain is exercised, not just the no-op path.)
- [ ] Migrations complete before any other code reads the DUET tree.
- [ ] A migrated project, once saved, is stamped N and reopens without migration.
- [ ] The application version is stamped into the engine's property storage on startup.
- [ ] The refusal is a user-visible dialog in the app, not only a facade error code.

## Notes

**agent** — 2026-08-24T06:51:40Z

Completed schema versioning at current schema 2. Every save stamps duetSchemaVersion. Open now returns a producer-facing refusal for newer schemas without writing the file; duet_app shows that refusal in a warning dialog. Load migrates in memory before Project is exposed, one version at a time: 0→1 introduces VIEW, then 1→2 versions that layout and renames projectNotes to sessionNotes. Migrated projects are dirty until saved, then reopen at schema 2 without migration. Startup stamps JUCE_APPLICATION_VERSION_STRING as duetApplicationVersion in Tracktion PropertyStorage. Facade tests cover current stamping, N+1 refusal and disk immutability, N-1, oldest-first N-2, and save/reopen. Final configure, format, full build, full lint, and all 176 tests pass (8 existing device-dependent skips).
