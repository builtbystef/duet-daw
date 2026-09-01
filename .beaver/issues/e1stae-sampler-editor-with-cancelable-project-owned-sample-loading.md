---
id: e1stae
title: Sampler editor with cancelable project-owned sample loading
state: todo
priority: high
labels:
    - session:task
    - roadmap:yfpnps
depends_on:
    - qzjn3u
    - c86xh8
    - ehdor9
parent: jt5rjt
created: 2026-09-01T18:35:37Z
updated: 2026-09-01T18:41:16Z
---

## Bounded implementation

Add the dedicated Graphite Sampler window over `SamplerZoneInfo` and the shared cancelable project-media worker.

## Settled interaction

- `Add Sample…` accepts the Browser-supported audio formats. It copies first; only a successful current-project result appends the default C3/full-range zone in one `Set Sampler Mapping` Action.
- Each zone row shows name/file status and three MIDI-note controls: Low, Root, High, as pitch name plus number. Edits enforce Low <= Root <= High and commit one Action per completed gesture; Remove and drag reorder are each one Action.
- A 128-key range strip visualizes zones; clicking a key sends bounded note-on/off through the completed Piano Roll audition seam `ehdor9`. This task does not invent a second audition implementation.
- Missing zones are labelled `Missing — <file>` with Locate and Remove. Locate imports the replacement and swaps only that zone in one Action.
- Sampler has Bypass and Close but no app-global preset controls; mappings belong to the project and reference its media.

## Acceptance and tests

- [ ] Loading, cancellation, stale-project completion, remap, reorder, remove, locate, and missing-file display obey the policies above.
- [ ] No Action refers to external media and no failed/canceled load changes the zone list.
- [ ] Component tests cover composition/focus/lifetime; model and worker tests prove state. A known zone is rendered from MIDI after save/reopen.

Start in `PluginEditorManager`, new Sampler editor components, and `Main.cpp` media-worker wiring. Run all AGENTS.md checks before closing.
