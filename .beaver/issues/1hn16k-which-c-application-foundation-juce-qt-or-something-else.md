---
id: 1hn16k
title: 'Which C++ application foundation: JUCE, Qt, or something else?'
state: done
assignee: claude
priority: high
labels:
    - roadmap:d9gioe
    - session:research
depends_on:
    - kimula
parent: d9gioe
created: 2026-08-07T06:01:55Z
updated: 2026-08-07T06:51:58Z
---

Research session. The one choice that spans GUI, audio I/O, and plugin hosting.

Evaluate against milestone-one requirements (node kimula): JUCE, Qt (+ separate audio layer), Dear ImGui-style immediate UI, and a custom stack (e.g. platform audio APIs + Skia/NanoVG). For each: audio I/O backend coverage (ASIO/WASAPI, CoreAudio, JACK/PipeWire), GUI capability for a dense DAW UI (custom widgets, timeline rendering performance), plugin-hosting support, licensing terms and cost (JUCE GPL/commercial split, Qt LGPL constraints), community/longevity, and how each constrains the AI-integration seam. Primary sources: official docs and licenses, not recall.

Deliverable: a recommendation with cited trade-offs, ready to become an ADR.

## Notes

**claude** — 2026-08-07T06:27:08Z

Constraint from the user (2026-08-07): the project starts as open source, with the potential to become a commercial product later. Evaluate every candidate technology, library, and service against that path — prefer permissive or dual-licensable licenses; a copyleft-only option with no commercial-license route (e.g. GPL with no paid tier) forecloses a commercial edition and needs explicit justification. Recorded in the roadmap root (d9gioe) under 'Licensing posture'.

**claude** — 2026-08-07T06:42:57Z

Research session closed (2026-08-07). All claims from primary sources; four parallel sub-agents (licensing, audio I/O, plugin hosting, GUI).

## Question

Which C++ application foundation — JUCE, Qt (+ a separate audio layer), a Dear ImGui-style immediate-mode UI, or a custom stack — for a Linux-first DAW that must host CLAP + VST3, render a dense timeline, and start open source with a commercial edition possible later (node kimula, roadmap d9gioe)?

## Answer

**JUCE 9.** It is the only candidate that covers all three axes this project needs from one dependency — audio I/O on every target platform, MIDI on every target platform, and VST3 plugin hosting — and it is the only candidate whose licence has a clean, documented open-source-now / commercial-later path that costs a purchase rather than a rewrite. Qt loses on substance, not licensing: it has **no MIDI at all** and **no JACK or ASIO backend**, so a Qt build would still need RtAudio/libremidi plus a hand-written VST3 and CLAP host — i.e. Qt buys only the GUI, at the price of its LGPL relink obligations and its explicit ban on mixing open-source and commercial Qt in one product. Dear ImGui is disqualified by its own README, which states it targets tooling "as opposed to UI for the average end-user" and does not support accessibility. A custom stack (Skia/NanoVG + SDL3 + RtAudio + libremidi) is all permissive and technically viable, but hands the project every windowing, text, IME, and hosting problem at once, for a solo developer whose milestone one is already a full DAW.

Two known costs of choosing JUCE, both accepted:

1. **CLAP hosting must be hand-built.** JUCE 9 hosts VST3/VST2/AU/LV2/LADSPA/ARA but has no CLAP support at all, and JUCE has never committed to CLAP *hosting*. This is not a JUCE-specific penalty — no candidate ships CLAP hosting, and the CLAP C API is MIT with an official `clap-helpers` host base class and a reference host to read. Detail passes to node hvv3nn.
2. **JUCE renders on Linux with the software renderer only.** The heavy surfaces (arrangement timeline, piano roll, waveforms, mixer meters) will need an attached `OpenGLContext` rather than the default path. Framework-supported, but it is real work and it must be proven in the walking skeleton (node ddp1qt).

The licensing verdict: ship the open-source project under **AGPLv3** (JUCE's open-source arm), and buy a per-user JUCE commercial licence if and when closed binaries are distributed. Nothing is re-architected at that point — it is a purchase. Qt's equivalent conversion is legally murkier: The Qt Company's own FAQ has an entry on exactly this conversion whose answer text is not retrievable from their site, and their terms forbid mixing the two licence models in one project.

## Findings

### Licensing — the decisive axis

- JUCE modules are "dual licensed under the AGPLv3 and the JUCE licence" — https://raw.githubusercontent.com/juce-framework/JUCE/master/LICENSE.md (master, 2026-08-07). **AGPLv3, not GPLv3** — network/service use also triggers disclosure. Relevant if a future AI feature ever runs JUCE-linked code server-side.
- The AGPLv3 route covers non-conveyed use: "If you are not using JUCE under the AGPLv3 then you will require a JUCE licence. You will need to maintain a licence for at least the duration over which you are distributing closed-source binaries containing JUCE." — https://juce.com/get-juce/ (2026-08-07).
- JUCE 9 tiers, per user: Starter free (revenue up to $20,000); Indie $40/month or **$800 perpetual** (up to $300,000); Pro $175/month or $3,500 perpetual (no cap); Educational free. Limits are "based on revenue received or obtained by the Licensee over the previous 12 months," including donations and sponsorship. — https://juce.com/get-juce/ and https://juce.com/legal/juce-9-licence/ (EULA 2026-06-17).
- **The JUCE splash screen is gone.** No splash, "Made with JUCE", attribution, or telemetry obligation appears anywhere in the JUCE 9 EULA — the historical JUCE 5 requirement no longer exists. — https://juce.com/legal/juce-9-licence/ (2026-06-17).
- Qt open source is LGPLv3 / GPLv2 / GPLv3. LGPLv3 on a distributed app requires a re-linking mechanism, "sufficient installation information" so the user can run the re-linked binary, a licence copy, explicit acknowledgement of Qt use, and a source copy available to customers. "In case of static linking… the application itself may no longer be 'work that uses the library' and thus become subject to LGPL." — https://www.qt.io/development/download-open-source and https://www.qt.io/licensing/open-source-lgpl-obligations (2026-08-07).
- **Qt forbids mixing licence models**: "Combining or mixing the Commercial Qt licensing and the Qt Community Edition within the same application or device development project is not allowed." — https://www.qt.io/licensing/ (2026-08-07).
- Qt commercial list price: Qt for Application Development Enterprise **$4,660/developer/year**; Small Business tier **$618/developer/year**, capped at annual revenue (incl. capital funding) below €1M and a maximum of three discounted developer licences. — https://www.qt.io/pricing and https://www.qt.io/terms-conditions/qt-dev-framework/exhibit-small-business-2026-01.
- Qt trap for a DAW: **Qt Charts and Qt Graphs are GPLv3-or-commercial only, with no LGPL option** — any built-in metering/spectrum display via those modules forces GPLv3 on the whole app. — https://doc.qt.io/qt-6/qtcharts-index.html and https://doc.qt.io/qt-6/licensing.html (Qt 6.11).
- Permissive, zero friction on either path: Dear ImGui MIT; Skia BSD-3-Clause; NanoVG zlib; CLAP MIT.
- **The VST3 SDK is now MIT** — "The SDK no longer offers GPLv3 or Steinberg proprietary licensing options." (LICENSE.txt © 2025 Steinberg; the change landed October 2025, after the 3.8.0 tag.) — https://github.com/steinbergmedia/vst3sdk. This removes what would have been the single largest licensing obstacle for a commercial-later DAW, and it removes a premise from node hvv3nn's brief.
- **ASIO is the one genuinely infectious dependency.** The free ASIO route is GPLv3 ("Steinberg ASIO technology is available in open source form under a GNU General Public License, version GPLv3"), which would force the whole DAW to GPLv3 on Windows; a closed Windows build needs the proprietary Steinberg agreement. — https://www.steinberg.net/developers/asiosdk-open/. This does not bind milestone one: Linux ships first, and JUCE's WASAPI backend covers Windows including exclusive and shared-low-latency modes.

### Audio and MIDI I/O

- JUCE 9.0.0 (released 2026-07-21) ships, per `modules/juce_audio_devices/native/`: ALSA, JACK, ASIO, WASAPI, DirectSound, CoreAudio, plus iOS/Android; MIDI for Linux (ALSA sequencer), macOS (CoreMIDI), Windows (Win32, optional WinRT and Windows MIDI Services for MIDI 2.0). — https://github.com/juce-framework/JUCE/tree/master/modules/juce_audio_devices/native.
- WASAPI covers all three modes: `enum class WASAPIDeviceMode { shared, exclusive, sharedLowLatency };` — https://github.com/juce-framework/JUCE/blob/master/modules/juce_audio_devices/juce_audio_devices.h (9.0.0). Defaults: `JUCE_ALSA 1`, `JUCE_WASAPI 1`, `JUCE_JACK 0`, `JUCE_ASIO 0` — JACK and ASIO are opt-in flips, not ports.
- JUCE 9 now bundles the ASIO headers ("By default, this option will use the bundled ASIO headers distributed alongside JUCE"), with the licence warning attached. — same header.
- **JUCE has no PipeWire backend**: a full recursive tree of JUCE master (5447 paths, not truncated) contains no PipeWire path. — https://api.github.com/repos/juce-framework/JUCE/git/trees/master?recursive=1 (2026-08-07). PipeWire is reached through its JACK (`pw-jack`) and PulseAudio compatibility layers — https://docs.pipewire.org/page_man_pw-jack_1.html. For a Linux-first DAW the practical path is JACK-via-PipeWire, i.e. flip `JUCE_JACK`.
- **Qt provides no MIDI at all** — MIDI appears nowhere in the Qt Multimedia overview, the audio overview, or the Qt 6.11 All Modules list. — https://doc.qt.io/qt-6/qtmultimedia-index.html, https://doc.qt.io/qt-6/qtmodules.html.
- Qt's low-level audio backends are exactly alsa (marked "experimental", autodetect off, available only when PulseAudio is absent), pulseaudio, pipewire, mmrenderer, coreaudio, aaudio, wasm, wasapi — **no JACK, no ASIO**. — https://github.com/qt/qtmultimedia/blob/6.11/src/multimedia/configure.cmake.
- Qt 6.10+ does have a native PipeWire backend (`src/multimedia/pipewire/`, absent on 6.8/6.9) — the only foundation-level native PipeWire audio support found. QAudioSink offers "a callback-based interface… allows much lower latency audio processing", but Qt publishes no latency figures; the QIODevice path buffers "typically 250ms". — https://doc.qt.io/qt-6/audiooverview.html.
- Non-JUCE stacks would take RtAudio 6.0.1 (ALSA/JACK/PulseAudio/OSS/CoreAudio/ASIO/WASAPI/DirectSound, MIT) or miniaudio 0.11.25 (public-domain/MIT-0, but **no ASIO**), plus libremidi 5.4.3 for MIDI (BSD-2; ALSA seq/raw, CoreMIDI, JACK, **native PipeWire**, WinMM, Windows MIDI Services, MIDI 2.0/UMP). libsoundio's last commit is 2023-07-06 (dormant); PortAudio's newest tag is v19.7.0 (2021-04-06). No surveyed audio I/O library has a native PipeWire backend.

### Plugin hosting

- JUCE 9 hosts VST3 off the shelf (`JUCE_PLUGINHOST_VST3`: "Enables the VST3 audio plugin hosting classes"), plus AU (macOS), LV2, LADSPA (Linux), ARA, and VST2 if you supply the SDK files. — https://raw.githubusercontent.com/juce-framework/JUCE/master/modules/juce_audio_processors_headless/juce_audio_processors_headless.h (9.0.0).
- **No CLAP in JUCE 9 at all**: `modules/juce_audio_processors/format_types/` has no CLAP file, and `juce_audio_plugin_client/` has no CLAP wrapper despite the Q3 2025 roadmap saying JUCE 9 would add CLAP *authoring*; CHANGE_LIST.md and the 9.0.0 release notes never mention CLAP. The roadmap statement covers authoring only — "JUCE 9 will add the ability to author CLAP plug-ins" — https://juce.com/blog/juce-roadmap-update-q3-2025/. No JUCE source commits to CLAP hosting.
- `clap-juce-extensions` does not close this gap: "It does not support JUCE-based CLAP hosting" (it wraps a JUCE plugin as a CLAP plugin). — https://raw.githubusercontent.com/free-audio/clap-juce-extensions/main/README.md. The only JUCE CLAP-hosting module found is `jatinchowdhury18/juce_clap_hosting`, self-described "super-alpha", 6 commits, MIDI/GUI/latency/state unimplemented.
- Hand-building CLAP hosting is well-supported: CLAP is an MIT C ABI with a binary-compatibility guarantee; `clap-helpers` provides `clap::helpers::Host`, a template base class wrapping the `clap_host` struct with virtuals per host extension and configurable misbehaviour handling (https://github.com/free-audio/clap-helpers); and `free-audio/clap-host` is an official MIT reference host (Qt6 + RtAudio/RtMidi) to read.
- The VST3 SDK ships real host-side machinery: `source/vst/hosting/` with per-platform module loading (`module_linux.cpp`/`module_win32.cpp`/`module_mac.mm`), `plugprovider`, `parameterchanges`, `eventlist`, `processdata`, `hostclasses`, plus four host sample apps. — https://github.com/steinbergmedia/vst3_public_sdk.
- **No permissively-licensed unified CLAP+VST3 hosting library exists.** DPF, iPlug2, and clap-wrapper are all authoring-side. The only unified hosting abstraction found is Carla, which is GPLv2+ (CLAP is supported in its backend — `PLUGIN_CLAP = 14` in `CarlaBackend.h` — though the README omits it). GPL rules Carla out under this project's licensing posture.

### GUI

- JUCE officially supports macOS 10.11+, Windows 10 v1607+, and "Mainstream Linux distributions" on x86_64/Arm64. — https://github.com/juce-framework/JUCE/blob/master/README.md.
- JUCE 8 made Direct2D "the default JUCE renderer on Windows", with "Both JUCE desktop windows and JUCE images… GPU-backed on Windows". — https://juce.com/blog/juce-8-feature-overview-direct-2d/.
- **On Linux, JUCE's window peer offers only the software renderer**: `getAvailableRenderingEngines()` returns `{ "Software Renderer" }`. — https://github.com/juce-framework/JUCE/blob/master/modules/juce_gui_basics/native/juce_Windowing_linux.cpp#L199 (master). On macOS the choices are Software and CoreGraphics — there is no Metal component renderer (juce_NSViewComponentPeer_mac.mm#L702). This is the load-bearing risk of the recommendation on the first platform.
- The escape hatch is framework-supported and per-component: attaching an `OpenGLContext` routes a component's `paint()` through an `OpenGLGraphicsContext`. — https://docs.juce.com/master/classjuce_1_1OpenGLContext.html. JUCE 9.0.0 also "Improved the performance of the software renderer" and "Added OpenGL ES support to Linux". — https://api.github.com/repos/juce-framework/JUCE/releases/tags/9.0.0.
- JUCE 8 replaced its text stack with HarfBuzz shaping and added variable-font support — https://juce.com/blog/juce-8-feature-overview-unicode/. Relevant because a custom stack on NanoVG would get stb_truetype glyph rasterisation, not shaping.
- Qt's own guidance: "Qt Widgets are for creating complex desktop applications", while "Qt Quick interfaces are fluid, dynamic, and are best on touch interfaces". — https://doc.qt.io/qt-6/topics-ui.html. Qt Quick is retained-mode and GPU-backed via QRhi (OpenGL/Vulkan/Metal/D3D), with custom drawing through `QQuickItem::updatePaintNode()` and QSG node trees; `QQuickPaintedItem` is the QPainter shim, flagged "It is important to understand the performance implications such items can incur." The Widgets-side large-canvas answer is QGraphicsView/QGraphicsScene with a BSP index and an optional QOpenGLWidget viewport. Qt is genuinely the stronger GPU rendering story — it is simply not enough to outweigh no-MIDI, no-JACK, no-hosting, and the licence terms.
- Dear ImGui rules itself out in its own README: "designed to enable fast iterations and to empower programmers to create content creation tools and visualization / debug tools (as opposed to UI for the average end-user)… full internationalization… and accessibility features are not supported." — https://github.com/ocornut/imgui/blob/master/docs/README.md. Text input is the application's job (`io.AddInputCharacter()`), and IME is manual per-platform wiring.
- Custom stack: Skia is BSD-3 with raster + Ganesh/Graphite GPU backends (Vulkan at feature parity with OpenGL, runtime-selectable), but builds Dawn via CMake and pulls ICU. **NanoVG's README states "This project is not actively maintained."** Neither supplies windowing, input, or text entry. GLFW explicitly is not "capable of rendering text" and is not a UI library; SDL3 is the better base because it has a real IME path (`SDL_StartTextInput()`, `SDL_EVENT_TEXT_EDITING`, and `SDL_TextEditingCandidatesEvent` since 3.2.0).

## Consequences for the rest of the roadmap

- **lf8tnt** (engine layer) — JUCE keeps Tracktion Engine on the table as the "adopt" option; evaluate its licence against the AGPLv3-now/commercial-later posture.
- **hvv3nn** (hosting) — two premises of its brief have changed: the VST3 SDK is MIT (the GPLv3/proprietary dual licence is gone), and CLAP hosting has to be written against the raw C API. Its real questions are now isolation, scanning, state, Linux/Windows UI embedding, and sidechain routing.
- **psmj4y** (toolchain) — JUCE 9 requires C++17 minimum (`minimumCppStandard: 17`) and expects CMake; `linuxPackages: alsa` is the declared Linux system dependency of `juce_audio_devices`.
- **ddp1qt** (walking skeleton) — must now explicitly prove the Linux rendering path: a scrolling, dense timeline surface under the software renderer, and the same with an attached `OpenGLContext`. If neither holds up, this recommendation is the thing to revisit.
- **Project licence** — choosing JUCE means the distributed open-source project is AGPLv3. That makes the Frontier's licensing item sharp enough to be a node, and it makes a CLA load-bearing: without one, contributed code cannot be relicensed for a commercial edition even after a JUCE commercial licence is bought. Added as a node.

## Unresolved

- **The Qt open-source → commercial conversion path.** The Qt Company's FAQ has an entry titled "I have started development of a product using the open source version of Qt, can I now purchase a commercial version of Qt and move my code under that license?" — the answer body is loaded client-side and is absent from both the rendered DOM and the server HTML at https://www.qt.io/faq/qt-open-source-licensing. Not chased further, because the verdict does not depend on it: Qt was rejected on missing MIDI, missing JACK/ASIO, and missing plugin hosting, not on this clause.
- **The proprietary Steinberg ASIO Licensing Agreement text** (terms, cost, redistribution). The link on https://www.steinberg.net/developers/prorietary-sdk/ is broken on Steinberg's own site — it points to the Game Audio Connect agreement. The real text ships inside the ASIO SDK zip. Deferred to the Windows port; milestone one is Linux, and WASAPI covers Windows without ASIO.
- **Why JUCE 9.0.0 ships without the roadmapped CLAP authoring support.** The Q3 2025 roadmap says JUCE 9 will add it; the 9.0.0 tree, changelog, and release notes show none. No JUCE statement reconciling the two was found. It does not change the verdict — no source anywhere claims JUCE CLAP *hosting*.
- **Qt Widgets' default paint engine** (whether QPainter widget rendering is hardware-accelerated in Qt 6) — Qt's paint-system docs name no default engine. Unverified, and moot given the verdict.
- **JUCE's `sharedLowLatency` minimum WASAPI period** — determined at runtime by `IAudioClient3`, no documented figure. A walking-skeleton measurement, not a research answer.

**claude** — 2026-08-07T06:51:58Z

Challenged and reaffirmed by the user (2026-08-07), after the recommendation was closed. The question raised: is JUCE's UI thorough enough for a whole DAW, and why not Qt, the more common C++ GUI choice?

REAFFIRMED: JUCE. The reasoning, so it is not re-litigated blind —

- Qt was not rejected on UI. It was rejected on everything that is not UI: no MIDI at all, no JACK and no ASIO backend, no plugin hosting (all verified from Qt's own module list and qtmultimedia's configure.cmake). 'Qt' was therefore never a foundation — it was always Qt + RtAudio + libremidi + a hand-written VST3 host + a hand-written CLAP host. JUCE leaves only the CLAP host to write.
- Qt IS the stronger GUI framework, and that is conceded, not disputed: real layout managers vs JUCE's manual resized() arithmetic; mature model/view, table views and docking vs JUCE's thin equivalents; and retained-mode GPU rendering on QRhi vs JUCE's software-only Linux peer. The counterweight is that a DAW's dominant surfaces — timeline, piano roll, waveforms, mixer, automation lanes — are bespoke custom-drawn canvases in every DAW, where both frameworks give the same thing: a rectangle, a paint callback, and mouse events. Qt's widget depth applies mainly to the chrome.

A GAP IN THIS RESEARCH, LEFT OPEN ON PURPOSE: the hybrid — Qt for the GUI, JUCE headless for audio and hosting — was never separately evaluated. It is more plausible than it used to be, since JUCE 9 split hosting config into a juce_audio_processors_headless module, and the official CLAP reference host is itself Qt6 + RtAudio/RtMidi. The unverified reasons it was not pursued: two application frameworks each expect to own the event loop and message thread, and that seam would sit permanently at the UI/audio-thread boundary; and the licensing is the worst of both — AGPLv3 from JUCE plus LGPLv3 from Qt, so a commercial edition needs BOTH a JUCE licence and a Qt licence (~$4,660/dev/yr list, $618 under the €1M small-business cap), against Qt's clause forbidding a project that mixes open-source and commercial Qt.

This is deliberately NOT recorded under Out of scope, because the user's decision was 'go with JUCE for now', not 'exclude the hybrid forever'. The trigger to revisit is node ddp1qt: if the walking skeleton shows JUCE's Linux rendering cannot carry a dense scrolling timeline, the hybrid is the first alternative to evaluate, and this note is the brief for it.
