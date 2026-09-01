---
id: jt5rjt
title: Built-in device editors and a playable Sampler
state: todo
priority: high
labels:
    - spec
    - roadmap:yfpnps
parent: yfpnps
created: 2026-09-01T18:07:43Z
updated: 2026-09-01T18:42:01Z
---

## What to build

Turn the five shipped Browser entries into editable devices. The 4OSC synth, EQ, compressor, and reverb open a Duet editor over their producer-facing parameters. The Sampler opens a dedicated editor that loads project-owned audio, assigns it to playable MIDI notes/ranges, and persists the assignment.

Today `PluginEditorAccess::processorOf` opens only an ExternalPlugin, so double-clicking a built-in does nothing; the engine Sampler also exposes no automatable parameters. This slice supplies the missing producer-facing model and components without widening the public model facade to JUCE or engine types.

## Acceptance criteria

- [ ] Double-clicking 4OSC, EQ, compressor, or reverb in a track's chain opens one editor window for that instance; reopening focuses the existing window.
- [ ] Each editor presents every parameter returned by the built-in parameter vocabulary with its producer-facing range, skew, units, display text, and current value.
- [ ] A control gesture is transiently audible and commits as exactly one Set Plugin Parameter Action at gesture end; Escape/cancel restores the original with no Action.
- [ ] Automation moves the same control on screen without creating an Action, and a manual gesture on an automated parameter follows the existing automation policy rather than silently fighting it.
- [ ] The Sampler editor accepts readable project-supported audio files, copies external files into the project's `audio/` directory before referring to them, and lets the producer assign each loaded sound a root note and playable note range.
- [ ] A loaded Sampler sound is audible at its assigned pitch/range from the Piano Roll and a hardware MIDI input; removing or remapping it is one named Action.
- [ ] Sampler mappings, tuning needed to play them, and sample references survive save/reopen; Save As remains self-contained and copies every referenced sample.
- [ ] Undo/redo restores built-in parameter edits and Sampler load/remap/remove operations digest-exactly; deleting a Sampler instance releases its editor and leaves no dangling read.
- [ ] Missing sample files are reported in the editor per zone while the rest of the project opens and the remaining zones continue to work.
- [ ] Built-in editors use Graphite chrome, the shared bypass and preset behavior where applicable, and no built-in device requires a VST3 or hidden automation lane to be configured.

## Architectural constraints

The public `duet_model` headers remain engine/JUCE-free. Prefer an engine-free built-in editor model over the existing parameter vocabulary. If component-only engine access is unavoidable for Sampler state, document a narrow exception beside ADR 0008 rather than extending ExternalPlugin access into a general plugin pointer.

## Testing seam

Parameter and Sampler gestures are asserted at the Action seam. Audio criteria render known MIDI through edited devices. Component tests cover editor lifetime and routing; no paint or screenshot tests are added.

## Notes

**agent** — 2026-09-01T18:42:01Z

AFK decomposition: 8ah7je -> otk1nr and 9mnzeh for parameterized built-ins; qzjn3u -> e1stae for Sampler, with e1stae also blocked by shared media worker c86xh8 and note audition ehdor9. This issue is a spec container.
