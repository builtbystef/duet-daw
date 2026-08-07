---
id: hvv3nn
title: 'How is CLAP + VST3 hosting built: SDK licensing, isolation, UI embedding?'
state: todo
priority: high
labels:
    - roadmap:d9gioe
    - session:research
depends_on:
    - 1hn16k
parent: d9gioe
created: 2026-08-07T06:25:15Z
updated: 2026-08-07T06:43:33Z
---

Research session. Milestone one hosts CLAP and VST3 plugins (node kimula). With the application foundation chosen (node 1hn16k — JUCE vs Qt changes the answer materially), settle how hosting is built:

- SDK licensing: the VST3 SDK's GPLv3/proprietary dual license, and how it interacts with the chosen foundation's license and the product's licensing plans; CLAP's MIT license.
- Hosting architecture: in-process vs out-of-process (crash isolation), plugin scanning, plugin state save/restore.
- Plugin UI embedding on Linux (X11/Wayland) and Windows — the platforms that ship first.
- Sidechain routing into plugins, since sidechaining is a milestone-one requirement (node kimula).

Primary sources: official SDK docs and licenses, not recall. Deliverable: a recommendation with cited trade-offs, ready to become an ADR.

## Notes

**claude** — 2026-08-07T06:27:08Z

Constraint from the user (2026-08-07): the project starts as open source, with the potential to become a commercial product later. Evaluate every candidate technology, library, and service against that path — prefer permissive or dual-licensable licenses; a copyleft-only option with no commercial-license route (e.g. GPL with no paid tier) forecloses a commercial edition and needs explicit justification. Recorded in the roadmap root (d9gioe) under 'Licensing posture'.

**claude** — 2026-08-07T06:43:33Z

Two premises of this brief changed at node 1hn16k (2026-08-07); read that node's note before starting.

1. THE VST3 SDK IS NOW MIT. 'The SDK no longer offers GPLv3 or Steinberg proprietary licensing options' — https://github.com/steinbergmedia/vst3sdk (LICENSE.txt (c) 2025 Steinberg; the change landed October 2025, after the 3.8.0 tag). The GPLv3/proprietary dual-licence question in this issue's body is moot. CLAP is MIT as before.

2. CLAP HOSTING MUST BE HAND-BUILT. JUCE 9 hosts VST3/VST2/AU/LV2/LADSPA/ARA but ships no CLAP code at all, and JUCE has never committed to CLAP hosting (its Q3 2025 roadmap covers CLAP authoring only). clap-juce-extensions states outright: 'It does not support JUCE-based CLAP hosting'. The only JUCE CLAP-hosting module found (jatinchowdhury18/juce_clap_hosting) is self-described 'super-alpha', 6 commits, with MIDI/GUI/latency/state unimplemented. No permissively-licensed unified CLAP+VST3 hosting library exists — DPF, iPlug2 and clap-wrapper are all authoring-side, and the only unified hosting abstraction (Carla) is GPLv2+, which the roadmap's licensing posture rules out.

Starting points for the hand-built path: the MIT CLAP C ABI (binary-compatible across 1.x), clap-helpers' 'clap::helpers::Host' template base class wrapping the clap_host struct with virtuals per host extension (https://github.com/free-audio/clap-helpers), and free-audio/clap-host, the official MIT reference host. On the VST3 side, vst3_public_sdk ships source/vst/hosting/ with per-platform module loading (module_linux.cpp/module_win32.cpp/module_mac.mm), plugprovider, parameterchanges, eventlist, processdata and hostclasses, plus four host sample apps.

So this session's real questions are the ones the body already lists minus the licensing one: in-process vs out-of-process isolation, plugin scanning, state save/restore, plugin UI embedding on X11/Wayland and Windows, and sidechain routing — plus how much of a CLAP host to write for milestone one.
