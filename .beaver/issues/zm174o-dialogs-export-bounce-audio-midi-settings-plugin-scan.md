---
id: zm174o
title: 'Dialogs: Export/Bounce, Audio & MIDI Settings, plugin scan'
state: done
assignee: claude
priority: medium
depends_on:
    - n6c30z
    - 6zog6s
    - sh2dkg
parent: 535bbo
created: 2026-08-12T03:52:00Z
updated: 2026-08-30T02:25:15Z
---

## What to build

The three dialogs the milestone still owes, all reached from the Duet menu.

Export/Bounce: name, destination, format, bit depth, sample rate, the range to render, and a normalize option, rendering through the foundation's offline render path with progress and a cancel.

Audio & MIDI Settings: the Audio and MIDI tabs join the Interface tab in the existing Settings window — output and input device, sample rate, buffer size, and the measured latency on one; the MIDI inputs to enable on the other.

The plugin-scan flow: start a scan, watch its progress, and see what it found — including the plugins the out-of-process scanner rejected, so a crashing plugin is reported rather than silently missing. A finished scan updates the browser's VST3 section without a restart.

## Acceptance criteria

- [ ] Export/Bounce offers name, destination, format, bit depth, sample rate, range and normalize, defaulting the range to the project's content and the name to the project's name.
- [ ] Export, worked: exporting bars 1–4 of a project playing a steady tone at 120 BPM produces a file at the chosen path whose duration is 8.0 seconds ± one render block and whose content is that tone; exporting the same range twice in one session produces identical output.
- [ ] Normalize on brings the exported peak to the target level; normalize off leaves levels as rendered.
- [ ] Export shows progress and can be cancelled; a cancelled export leaves no partial file behind, and the app stays responsive throughout — the render does not run on the message thread.
- [ ] Exporting does not disturb the session: the transport, the selection and the undo stack are unchanged afterwards, and the document is not dirtied.
- [ ] The Audio tab changes output device, sample rate and buffer size live, shows the resulting latency, and its choices persist across restarts as app-global settings; a device that fails to open reports it in the dialog and leaves the previous device running.
- [ ] The MIDI tab lists inputs, enables and disables them, and an enabled input plays the armed track's instrument.
- [ ] Starting a plugin scan shows progress with the plugin being scanned; the scan runs out of process, and a plugin that crashes the scanner is reported in the results rather than taking the app down.
- [ ] A finished scan updates the browser's VST3 section without a restart, and rescanning does not duplicate the plugins already found.
- [ ] All three dialogs follow the Graphite tokens in both modes and are keyboard-dismissible.

## Notes

**claude** — 2026-08-29T11:21:38Z

Seams for this slice (spec 535bbo names the two the interface uses; ADR 0006 governs the audio ones):

- The export's audio criteria at the model seam, through the RenderHarness: a render measured on its features, and the within-process determinism canary for 'identical output'.
- Everything the dialogs decide at the view-model seam: the paintless Export, AudioMidiSettings and PluginScan view-models in duet_gui, driven directly.
- The devices behind the Audio and MIDI tabs through an interface duet_gui declares and duet_app implements over the model, the way Settings already is: a headless machine has no audio hardware, so what the tab does with a device is asserted against a fake and what the adapter does with the engine is asserted where an engine exists.
- The components in the JUCE-linked gui suite: that the dialogs exist, are dismissible, and read the same view-models.

**claude** — 2026-08-30T02:25:15Z

Completed 2026-08-30. The unattended iteration that built this was killed at the loop's two-hour ceiling after the implementation was finished but before its checks ran, so the work sat uncommitted; this session verified and finished it.

Two things the checks found and this commit fixes: a const_cast stripping const in PluginScanTests (the local was const while setSession takes a Session*), and two cases that asserted found().size() == 1. found() reads the app-global known-plugin list — which is what lets a scanned plugin survive a restart — so its size counts what every other scan in the same process left behind. Those two were the only cases in 509 that depended on ctest forking per test; they now count occurrences of their own fixture, which keeps criterion 9's 'rescanning does not duplicate' intact.

Verified: build clean, ctest 554/554, clang-format clean, full lint sweep clean.

Also worth recording for whoever measures next: the whole-tree lint sweep now takes over twelve minutes on the dev machine, not the ~80 s AGENTS.md still records.
