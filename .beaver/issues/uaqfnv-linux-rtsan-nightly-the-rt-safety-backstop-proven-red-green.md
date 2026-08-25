---
id: uaqfnv
title: 'linux-rtsan nightly: the RT-safety backstop, proven red/green'
state: done
assignee: claude
priority: medium
depends_on:
    - 3u1blw
    - 6zog6s
parent: b1j3me
created: 2026-08-11T01:52:13Z
updated: 2026-08-25T18:33:09Z
---

## What to build

The third nightly per spec b1j3me / ADR 0006 (nodes bd11an, ox1trt): a linux-rtsan configuration on Clang 20.1.8+ with compiler-rt — RealtimeSanitizer is driver-incompatible with ASan/TSan/UBSan/MSan, so it cannot join the existing nightlies, and the Clang 18 floor governs what builds Duet, not what lints it. Every Duet callback target and the render-test executable compile and link with the realtime sanitizer. Callback entry points — the JUCE-hosted processor's process callback and the native plugin's apply-to-buffer — are noexcept and carry the nonblocking annotation through a feature-tested project macro (the rules live in the Real-time audio section of docs/CODING_STANDARDS.md). Since no production Duet callback code exists yet, this slice creates a minimal test-only processor at each seam; offline rendering (the render harness) enters both seams with no audio device, which is what makes the nightly meaningful headlessly. The spec mandates that this first slice proves the preset red/green.

## Acceptance criteria

- [ ] A linux-rtsan CMake preset/config exists, building with Clang 20.1.8+ and the realtime sanitizer on every Duet callback target and the render-test executable; the regular GCC 13 / Clang 18 builds are unaffected.
- [ ] A feature-tested project macro applies the nonblocking annotation where supported and compiles to nothing elsewhere; both test-processor entry points are noexcept and annotated, and the sanitizer test enters through those annotations.
- [ ] A minimal test-only processor exists at each seam — one JUCE-hosted (process callback) and one engine-native (apply-to-buffer) — and both are exercised headlessly by offline render in the rtsan config.
- [ ] Red/green proof, recorded as a note on this issue: inject one heap allocation into each seam's callback → the rtsan test run fails for each; remove them → it passes. Both runs' evidence (local or CI) is linked or quoted.
- [ ] CI runs linux-rtsan as a third independent nightly job alongside the ASan+UBSan and TSan+UBSan nightlies.
- [ ] No sanitizer suppressions for Duet code; any exact upstream-function suppression carries a documented false positive (per the coding standards).

## Notes

**claude** — 2026-08-17T04:12:26Z

Scope note (2026-08-17): acquiring the Clang 20.1.8+ toolchain on the ubuntu-24.04 runner is part of this slice (for example a pinned LLVM apt source — verify the exact mechanism at implementation and record it in the closing note). The Clang 18 floor still governs what builds Duet; 20.1.8+ is the lint toolchain only.

**claude** — 2026-08-25T18:33:09Z

Done. The `linux-rtsan` nightly exists, is proven red and green at both callback
seams, and the ordinary builds are untouched (281/281 under `linux-debug`,
281/281 under `linux-rtsan`, same 8 device-needing skips in both; format and
lint clean).

**Where the flag lives, and why not in the preset.** A new interface module,
`duet_realtime`, carries two things that are one decision: the header with
`DUET_NONBLOCKING` — feature-tested on `__has_cpp_attribute(clang::nonblocking)`,
nothing on GCC 13 and Clang 18 — and, under `DUET_RTSAN`,
`-fsanitize=realtime -fno-omit-frame-pointer` as interface compile and link
options. A Duet target holding callback code links `duet::realtime`, and that
link is what puts it under the sanitizer, so "every Duet callback target" is a
fact about the build graph rather than a list that can fall out of date;
`git grep duet::realtime` is the list, and `duet_tests` gets both the
instrumentation and the runtime transitively through the probes. The preset sets
only the compiler and `DUET_RTSAN=ON`. The root CMakeLists refuses `DUET_RTSAN`
on anything below Clang 20.1.8, so the floor is a sentence rather than an
unknown-flag error deep in a build.

**Two attribute facts worth having.** `[[clang::nonblocking]]` appertains to the
function *type*, so it goes after the parameter list and its `noexcept` and
before `override` — put in front of the declaration it is a hard error
("attribute cannot be applied to a declaration"). And it must be repeated on
every redeclaration. Both are in the header's own comment.

**The two probes** live in `tests/rt_probes` and are test-only, milestone one
having no Duet-authored DSP. The engine-native one is a
`tracktion::engine::Plugin` registered with `createBuiltInType` and inserted at
the head of a track's chain with no undo manager (test scaffolding is not a
producer gesture). The JUCE-hosted one is a VST3 built with `juce_add_plugin`,
because `ExternalPlugin` is the only way the engine reaches a
`juce::AudioProcessor`: it is scanned and inserted through the vocabulary
exactly as a producer's plugin is, which is what leaves `processBlock` as the
entry with no Duet frame above it. Both halve the audio, and both cases in
`tests/RealtimeSafetyTests.cpp` measure that -6 dB in an offline render — a
probe that stops being entered is the one failure a sanitizer cannot see, since
a callback that never runs looks exactly like a green nightly.

**A sanitizer-instrumented shared library does not carry the runtime.** Clang
links it into executables only and leaves `__rtsan_realtime_enter/_exit` and
`__rtsan_ensure_initialized` undefined in a module, for whatever host dlopens it
to resolve — which is right here, the runtime being in `duet_tests`. JUCE ends
every plugin link with `-Wl,--no-undefined`, so the probe's VST3 target appends
`-Wl,-z,undefs` after it, under `DUET_RTSAN` only. Verified: the bundle's three
rtsan symbols are `U`, and the red run below symbolizes straight through the
dlopen boundary.

**Red/green proof, on the dev machine, 2026-08-25.** One `std::vector<float>`
allocation injected into each callback in turn, built and run under
`linux-rtsan`, then removed.

Engine-native seam — `./build/linux-rtsan/tests/Debug/duet_tests "an offline
render enters the engine-native probe's applyToBuffer"` exited **43**:

```
==29==ERROR: RealtimeSanitizer: unsafe-library-call
Intercepted call to real-time unsafe function `malloc` in real-time context!
    #0  malloc
    #1  operator new(unsigned long)
    ...
    #8  std::vector<float, std::allocator<float>>::vector(...)
    #9  (anonymous namespace)::NativeRealtimeProbe::applyToBuffer(tracktion::engine::PluginRenderContext const&)
          tests/rt_probes/NativeRealtimeProbe.cpp:52:28
    #10 tracktion::engine::Plugin::applyToBufferWithAutomation(...)  tracktion_Plugin.cpp:753
    #11 tracktion::engine::PluginNode::process(...)                  tracktion_PluginNode.cpp:253
```

JUCE-hosted seam — the same for `"an offline render enters the JUCE-hosted
probe's processBlock"`, also **43**:

```
==39==ERROR: RealtimeSanitizer: unsafe-library-call
Intercepted call to real-time unsafe function `malloc` in real-time context!
    #0  malloc
    ...
    #8  (anonymous namespace)::RealtimeProbeProcessor::processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&)
          tests/rt_probes/JuceRealtimeProbe.cpp:51:28
    #9  juce::JuceVST3Component::processAudio<float>(...)            juce_audio_plugin_client_VST3.cpp:3733
    #11 juce::VST3PluginInstanceHeadless::processAudio<float>(...)   juce_VST3PluginFormatImpl.h:2487
    #13 tracktion::engine::ExternalPlugin::processPluginBlock(...)   tracktion_ExternalPlugin.cpp:1495
    #14 tracktion::engine::ExternalPlugin::applyToBuffer(...)        tracktion_ExternalPlugin.cpp:1403
```

Both stacks bottom out in the annotated function with nothing of Duet's above
it, which is the thing the annotation had to be shown to do. With the two
allocations removed: `ctest --preset linux-rtsan` → **281/281 passed**, 90 s.

**The toolchain, as the scope note asked.** apt.llvm.org is the mechanism, and
`llvm-toolchain-noble-20` currently carries exactly 20.1.8
(`1:20.1.8~++20250804090239+87f0227cb601-1~exp1~...`). The nightly job adds that
source, installs `clang-20 libclang-rt-20-dev llvm-20` — compiler, the
compiler-rt runtimes the flag links against, and the symbolizer without which a
report names addresses instead of a callback — and runs with
`RTSAN_OPTIONS=halt_on_error=1:external_symbolizer_path=/usr/bin/llvm-symbolizer-20`.
AGENTS.md records the same recipe for the dev machine, plus the no-root
fallback: `dpkg-deb -x` those three packages into a prefix and put its `usr/bin`
on the `PATH`, which is what this session used — the binaries there carry the
`-20` suffix the preset asks for and find their libraries and runtimes
themselves. `rtsan` is a third job rather than a third matrix entry precisely
because it is the one that installs a compiler first.

**No suppressions**, for Duet or for anyone: a clean run needed none.

Docs: ARCHITECTURE.md gains `duet_realtime`; CODING_STANDARDS.md's callback rule
now names `DUET_NONBLOCKING` and where it lives; AGENTS.md's CI section covers
three nightlies instead of two.

**Not done here, noted:** `-Wfunction-effects` — Clang 20's compile-time side of
the same attribute — is still off, as bd11an's closing note recommended. It is a
separate adoption with its own diagnostics to measure, and the runtime verdict
this issue asked for does not depend on it.
