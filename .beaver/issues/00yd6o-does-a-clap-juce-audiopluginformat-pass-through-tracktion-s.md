---
id: 00yd6o
title: Does a CLAP juce::AudioPluginFormat pass through Tracktion's ExternalPlugin unchanged?
state: todo
priority: high
labels:
    - roadmap:d9gioe
    - session:prototype
depends_on:
    - 89jlz1
    - ddp1qt
parent: d9gioe
created: 2026-08-08T03:51:55Z
updated: 2026-08-08T03:51:55Z
---

Prototype session (disposable code). Node hvv3nn's recommendation rests on one claim that was read from source but never executed: that Tracktion's `ExternalPlugin` is format-agnostic enough that a foreign `juce::AudioPluginFormat` becomes a first-class plugin in an `Edit` with sidechain, state, automation, bus layout and editor windows all working unchanged. The in-tree precedent (Tracktion's own Cmajor patch format, registered through the same public seam at `tracktion_PluginManager.cpp:74-77`) makes it very likely. "Very likely" is not what a milestone-one plugin layer should rest on, and if it is wrong the fallback — subclassing `tracktion::engine::Plugin` directly — is a much larger build that changes the foundation spec.

Build the thinnest possible CLAP `juce::AudioPluginFormat` + `juce::AudioPluginInstance` covering only the extension set node 89jlz1 settles, register it into `PluginManager::pluginFormatManager`, and answer:

- Does a real third-party CLAP plugin scan, appear in the KnownPluginList, instantiate as an `ExternalPlugin` in an `Edit`, and process audio?
- Does its GUI embed? On Linux this is `clap_window{ .api = CLAP_WINDOW_API_X11, .x11 = juce::XEmbedComponent::getHostWindowID() }` -> `set_parent()`, with `clap_host_posix_fd_support` and `clap_host_timer_support` hand-wired to `juce::LinuxEventLoop::registerFdCallback` and `juce::Timer`. Test with a plugin whose GUI is not JUCE-based.
- Does state round-trip through save/reload of the Edit — i.e. does `getStateInformation` marshalling `clap_plugin_state::save` over a `MemoryBlock` stream survive Tracktion's base64 into `IDs::state`?
- Does sidechain work end to end? Expose the CLAP sidechain port as a *second JUCE input bus* so total input channels exceed 2, then check that `Plugin::canSidechain()` returns true, that the track appears in `getSidechainSourceNames`, and that audio actually arrives on it. This is the single most fragile link in the chain: Tracktion never asks whether a bus is a sidechain, only whether the channel count is large enough.
- Do plugin parameters appear as automatable Tracktion parameters, and does automation write and play back?

Also confirm the two known gaps hvv3nn identified are as small as they look: `pluginFormatName` is not persisted in the Edit (fix candidate: `EngineBehaviour::findDescriptionForFileOrID`), and out-of-process scanning excludes custom formats twice over (`tracktion_PluginScanHelpers.h:321-331` allowlists VST/AU/LADSPA by name, and the scan child process only registers default formats). Establish whether patching that header is the practical route or whether CLAP scans in-process for milestone one.

Read hvv3nn's closing note first — every file and line reference needed is in it. Depends on ddp1qt because this needs a building JUCE 9 + Tracktion Engine project to sit in.

Deliverable: a verdict on the seam, the list of what did not come free, and the corrected cost estimate for the CLAP host. The code is thrown away. If the seam does not hold, say so plainly — the foundation spec's plugin-hosting section depends on this answer, not on hvv3nn's.
