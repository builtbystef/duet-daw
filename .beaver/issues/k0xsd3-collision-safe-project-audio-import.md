---
id: k0xsd3
title: Collision-safe project audio import
state: done
assignee: agent
priority: high
labels:
    - session:task
    - roadmap:yfpnps
parent: kmb4mv
created: 2026-09-01T18:34:33Z
updated: 2026-09-01T23:32:16Z
---

## Bounded implementation

Make `duet::persistence::Project::importAudioFile` safe for every later audio/Sampler import. This task is synchronous file work only; callers that originate in the UI run it on a worker in later tasks.

## Settled naming policy

- Importing a file already inside this project's `audio/` returns that file without copying.
- If `audio/name.ext` is absent, use it.
- If it exists and is byte-identical (size, then chunked byte comparison), reuse it.
- If it differs, choose the first free `name 2.ext`, `name 3.ext`, and so on. Never overwrite existing content.
- Copy to a sibling `.partial` and atomically rename it. Any failure removes the partial and returns an empty path.

## Acceptance and tests

- [ ] Same file and same-content imports reuse one project file.
- [ ] Different files with one name remain byte-exact under deterministic suffixes.
- [ ] A failed/interrupted copy leaves neither a destination nor `.partial` debris.
- [ ] Existing project-relative source references and Save As behavior remain unchanged.
- [ ] Tests use real temporary files through the public `Project` seam in `tests/ProjectTests.cpp`.

Start in `modules/duet_persistence/src/Project.cpp`. Run targeted tests red/green, then all AGENTS.md checks before closing.

## Notes

**agent** — 2026-09-01T22:47:54Z

Seam: public Project::importAudioFile in tests/ProjectTests.cpp, as the issue named. Real temporary files only; no engine types.

**agent** — 2026-09-01T23:32:16Z

Collision-safe import at Project::importAudioFile.

A file already under this project's audio/ is returned without copying. Absent names are used as-is. Byte-identical collisions (size, then chunked comparison) reuse the existing file; different content takes the first free `name 2.ext`, `name 3.ext`, and so on. Copy writes a sibling `.partial` and renames onto the destination; any failure removes the partial and returns empty, and a leftover partial from an interrupted copy is cleared by the next import of that name. Existing project-relative clip references and Save As are unchanged.

Tests are in tests/ProjectTests.cpp through the public Project seam with real temporary files. Format, lint, and the full linux-debug suite pass.
