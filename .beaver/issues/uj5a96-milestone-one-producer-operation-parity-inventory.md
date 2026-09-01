---
id: uj5a96
title: Milestone-one producer-operation parity inventory
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
depends_on:
    - fsaacq
    - np7wjh
    - cbc13c
    - 1bo4s7
parent: kkclj0
created: 2026-09-01T18:38:30Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Create and enforce a source-controlled inventory that maps milestone-one operations to their direct producer route. This closes the gap in which the Edit Vocabulary can advance beyond the interface unnoticed.

## Settled artifact and guard

- Add `docs/PRODUCER_OPERATIONS.md` with one row per producer-meaningful operation: domain term, Action/configuration name, public model method, visible route, keyboard route where one exists, and Suggestion operation if applicable.
- Generate/maintain a small compile-time inventory beside the Edit Vocabulary. A test fails when a Suggestion edit operation lacks a direct producer route id or when a route id has no registered component/command.
- Configuration-only operations (transport, input, monitoring, arm, view state) are listed and explicitly marked non-Action. External-plugin-only vendor operations are not invented.
- The document contains no aspirational route: every listed component id/command is present in the shipping build and exercised by a focused test.

## Acceptance and tests

- [ ] Group, sends, sidechains, sections, plugin parameters, Sampler mappings, import, recording configuration, arrangement edits, MIDI edits, save/export, and Collaborator acceptance all have real rows.
- [ ] A deliberately unregistered Suggestion operation makes the parity test fail; restoring its route makes it pass.
- [ ] No operation remains reachable only through Collaborator JSON, a fixture, or a direct model call.

Run all AGENTS.md checks before closing.
