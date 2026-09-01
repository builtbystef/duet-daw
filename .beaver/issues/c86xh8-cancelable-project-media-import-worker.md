---
id: c86xh8
title: Cancelable project-media import worker
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
depends_on:
    - k0xsd3
parent: kmb4mv
created: 2026-09-01T18:34:54Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Provide the one host-owned worker service used by arrangement audio import and Sampler loading. It wraps `Project::importAudioFile`; it does not create clips, zones, or Actions.

## Settled contract

- A request carries source path plus the current project generation and reports queued/running byte progress, success with project-owned path, canceled, or plain failure on the message thread.
- Copy/comparison runs on one background worker. Cancellation and project replacement suppress stale completion; the persistence helper cleans its own `.partial` file.
- Destruction cancels, joins without blocking the audio callback, and delivers no callback into destroyed UI.
- Requests for the same source/project may serialize but never race to overwrite a destination.

## Acceptance and tests

- [ ] A commandable executor proves success, progress, cancellation, stale-generation suppression, and lifetime safety without sleeps.
- [ ] Neither message-thread paint nor the audio callback performs file I/O.
- [ ] The public service interface contains filesystem/progress values only—no engine type.

Place ownership in `duet_app` beside project lifecycle/import wiring, with the smallest engine-free callback interface needed by `duet_gui`. Run all AGENTS.md checks before closing.
