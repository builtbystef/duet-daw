---
id: yfpnps
title: Usable DAW workflow — milestone-one completion
state: todo
priority: high
labels:
    - spec
    - roadmap:yfpnps
created: 2026-09-01T18:05:51Z
updated: 2026-09-01T18:42:01Z
---

## Problem Statement

The Interface specification (535bbo) has all of its original implementation slices closed, but the resulting application is feature-complete only along narrow tested paths. Several capabilities promised by milestone one exist in `duet_model` or in the Edit Vocabulary without a complete producer-facing route, and several editing surfaces lack the feedback and direct manipulation needed to use those routes fluently.

The practical consequence is that the Target Producer can make and export a simple default-synth phrase, but cannot complete the milestone-one workflow without omissions or hidden help: configure arbitrary tracks for recording, edit the built-in devices or load the Sampler, create and mix through groups/sends/sidechains, choose a loop range, perform ordinary Piano Roll clipboard work, or audition and directly import source material.

This specification is the usability-completion area after 535bbo. It does not reopen Graphite, the single-window layout, the Action seam, the view-model/component split, or any existing ADR.

## Solution

Complete one producer-visible path through every milestone-one workflow and validate the paths together rather than as isolated controls. The seven implementation slices are:

1. Per-track input, monitoring, routing, and record controls.
2. Built-in synth/effect editors plus a functional Sampler.
3. Group tracks, sends, and sidechain routing.
4. Editable loop range, section bands, and arrangement drag feedback.
5. Piano Roll ruler/grid, clipboard, and note audition.
6. Sample audition plus external audio/MIDI import.
7. A final discoverability and workflow pass.

The area ends at one release gate: starting from a fresh launch, the Target Producer can build an eight-bar part with a synth and samples, record MIDI and audio, set and use the loop, create a reverb send and a sidechain, mix the result, save and reopen it, and export it. Every act is performed through visible interface affordances, without direct model calls, fixture setup, or asking the Collaborator to reach an operation the producer cannot.

## User Stories

1. As the Target Producer, I can choose what each track records, hear the chosen monitoring mode, arm it, and understand its routing before I start a take.
2. As the Target Producer, I can shape the shipped synth and effects and load playable samples into the shipped Sampler, so the built-ins are instruments and devices rather than Browser labels.
3. As the Target Producer, I can create groups, post-fader sends, and sidechains from the mixer, so the complete milestone-one mix graph is reachable by hand.
4. As the Target Producer, I can draw the loop I mean, read named sections, and see where a clip edit will land before I release the pointer.
5. As the Target Producer, I can read musical time and use ordinary clipboard and audition gestures in the Piano Roll.
6. As the Target Producer, I can hear a sample before choosing it and bring audio or MIDI into the project directly from the desktop.
7. As the Target Producer encountering a control or an empty/error state for the first time, I can tell what it does and what to do next without reading source code or a test.

## Implementation Decisions

- The existing module boundaries remain binding. Paintless view-models stay in `duet::gui`; JUCE components stay in `duet::gui_components`; every project edit still ends in one named Action through `duet_model`.
- The public `duet_model` facade remains free of JUCE and engine types. Any component-only access needed by a built-in editor must be as narrow and explicit as ADR 0008, not a general engine escape hatch.
- Producer parity is a completion criterion. A milestone-one operation exposed to the Collaborator must also have a direct producer-facing route. In particular, group creation, post-fader sends, and sidechain assignment cannot remain Suggestion-only operations.
- Recording configuration follows the model's existing semantics: input choice, monitoring, and record arm do not enter producer undo; output routing and mixer edits are Actions.
- Imported material preserves the self-contained project rule of ADR 0005. Audio used by a clip or Sampler is copied into the project folder before project state refers to it. A MIDI import materializes as ordinary MIDI clips and notes in the Edit Vocabulary.
- Loop-range and section editing share the arrangement's timeline geometry and snap policy. They are not separate coordinate systems.
- Auditioning source material is transport-independent and does not alter the project or its undo history.
- Visual feedback during a drag is transient view state. The completed gesture emits one Action; Escape or an invalid drop emits none.

## Testing Decisions

Each slice uses the existing Action and paintless view-model seams. Component tests cover only behavior that requires a real component: focus, pointer routing, desktop file drops, keyboard dispatch, editor hosting, and visible control composition. Audio claims use ADR 0006 feature assertions.

The area also has one manual end-to-end gate. Unit and component coverage cannot replace it because its question is whether the controls form a discoverable workflow when used together.

## AFK implementation contract

This specification and its six `spec`-labelled slice containers are not executable work. An unattended implementation loop claims only ready leaves carrying both `roadmap:yfpnps` and `session:task`:

```bash
beaver list --ready --label roadmap:yfpnps --label session:task
```

One iteration takes the first returned issue, runs `beaver start <id>`, implements it red → green, runs its targeted checks and the full `AGENTS.md` gate, then runs `beaver done <id>` and commits code plus issue state. If it cannot satisfy the written acceptance criteria, it notes the exact blocker and releases the issue instead of guessing or weakening the criteria. An empty result means stop for a dependency or human review.

Run one implementation worker at a time: several ready roots deliberately touch `Session.h`, `EditOps.cpp`, or the main canvases, and concurrent workers would create merge conflicts rather than useful parallelism. Each leaf settles its remaining product behavior, names the outer public test seam and starting files, excludes physical-device or visual approval, and requires the repository checks in `AGENTS.md` before closure. Issues labelled `review` are deliberate human stops and must never be claimed by that loop. A failed implementation is noted and released rather than weakening acceptance criteria.

## Release Gate

On the development machine, from a fresh project and through the shipping app:

1. Create an eight-bar loop range.
2. Create and edit a synth part.
3. Find and audition samples, import them, and make a playable Sampler part.
4. Select a MIDI input, record a MIDI take, and edit it in the Piano Roll.
5. Select an audio input and monitoring mode, record an audio take, and hear monitoring while it rolls.
6. Create a group/reverb bus and a post-fader send.
7. Add a compressor and choose a sidechain source.
8. Mix through the visible controls, save, close, reopen, and verify the routing and device states return.
9. Export the eight-bar result and verify the written file is audible and the expected length.

No step may use a test fixture, direct model API, hand-edited project data, or the Collaborator as a substitute for missing producer controls.

## Out of Scope

The following are desirable daily-use follow-ons, not blockers for this immediate milestone-one usability area: clip splitting, clip fades and crossfades, clip gain, and recording count-in. They are tracked separately rather than silently folded into the release gate.

The exclusions already settled by 535bbo remain excluded: comping, punch-in, loop recording, recorded automation modes, pre-fader sends, external hardware routing, CLAP, and the session/clip-launch grid.

## Notes

**agent** — 2026-09-01T18:42:01Z

Prepared for unattended implementation: claim only ready issues with labels roadmap:yfpnps + session:task. Spec containers and review stops are excluded. Current graph ends at parity task uj5a96 plus live recording review 0x49el, then review kkclj0 and manual gate jpv27l.
