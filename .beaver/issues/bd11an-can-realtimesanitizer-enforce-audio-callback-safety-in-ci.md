---
id: bd11an
title: Can RealtimeSanitizer enforce audio-callback safety in CI, and what is the RT-safety coding standard?
state: done
assignee: agent
priority: medium
labels:
    - roadmap:d9gioe
    - session:research
depends_on:
    - psmj4y
parent: d9gioe
created: 2026-08-10T23:44:00Z
updated: 2026-08-11T00:08:14Z
---

Milestone one's audio-thread code is Duet's own: two built-in instruments (synth, sampler) and three effects (EQ, compressor, reverb). The standing rule is "no allocations, no locks, no syscalls in the audio callback," enforced by coding standards + review (settled at the Frontier-sharpening session, 2026-08-10). The question: can CI enforce it too?

**RealtimeSanitizer (RTSan) landed in LLVM 20; the settled secondary compiler is Clang 18 (node psmj4y).** Find out, from primary sources and a hands-on check where possible:

- Does RTSan work on the settled toolchain, or does it need a newer Clang than the floor? A *nightly* sanitizer config may use a newer Clang than the floor compiler — the floor governs what builds Duet, not what lints it. Does RTSan compose with the existing nightly configs (ASan+UBSan, TSan+UBSan), or does it need its own config?
- What does RTSan actually catch in a JUCE/Tracktion host — does instrumenting Duet's processor callbacks work when the callback is invoked from an uninstrumented engine? How are `[[clang::nonblocking]]` annotations applied at the `processBlock` seam?
- False positives / suppression story for the vendored trees.
- Whether a headless render (no real audio device) exercises the callback path enough for RTSan to be meaningful in CI.

**Deliverable is double** (settled at the sharpening session): (1) the feasibility answer — if RTSan works, the concrete nightly-config addition; if not, the reason and what the fallback is (debug-build assertion hooks were option (c), discipline-only is the floor). (2) The RT-safety section of `docs/CODING_STANDARDS.md` — what audio-callback code may not do — written by this session, since it will have just enumerated exactly what RTSan does and does not catch.

Trust boundary (settled): Tracktion Engine is trusted outright; only Duet's own code is in scope.

## Notes

**agent** — 2026-08-11T00:08:04Z

## Question

Can RealtimeSanitizer enforce the no-allocation, no-lock, no-syscall rule for Duet audio callbacks in CI, and what coding standard must govern the paths it cannot prove safe?

## Answer

Yes. Add a third independent nightly preset and job, `linux-rtsan`, using Clang 20.1.8 or newer with compiler-rt. Compile every Duet callback target with `-fsanitize=realtime`, link the headless render test executable with the same flag, and leave the existing ASan+UBSan and TSan+UBSan jobs unchanged: the Clang 20 driver declares RTSan incompatible with Address, Thread, Undefined, and Memory sanitizers.

Annotate every Duet audio entry point with a portable macro expanding to `[[clang::nonblocking]]`, and declare it `noexcept`. For native Tracktion plugins that entry is `Plugin::applyToBuffer`; for JUCE-hosted processors it is `AudioProcessor::processBlock`. Tracktion may remain uninstrumented. Offline rendering enters both seams without a real audio device, so CI can exercise them meaningfully.

The durable human rule is now recorded in `docs/CODING_STANDARDS.md`. RTSan is a dynamic backstop, not proof: review still enforces bounded work, prepared memory, no locks or waits, no operating-system I/O, and bounded lock-free communication.

## Findings

### Toolchain and configuration

- RTSan first shipped in Clang 20.1 and is enabled by `-fsanitize=realtime`. The settled Clang 18 floor cannot provide it. A local probe on Ubuntu Clang 18.1.3 rejected that flag as unsupported. [Clang 20.1 release notes](https://releases.llvm.org/20.1.0/tools/clang/docs/ReleaseNotes.html#sanitizers), 2025.
- Linux x86_64 is an RTSan-supported architecture. [LLVM 20.1.8 supported-architecture definitions](https://github.com/llvm/llvm-project/blob/llvmorg-20.1.8/compiler-rt/cmake/Modules/AllSupportedArchDefs.cmake), 2025.
- Both the annotated Duet translation unit and the final executable link require `-fsanitize=realtime`; a violation exits non-zero by default. [Clang 20.1 RTSan documentation](https://releases.llvm.org/20.1.0/tools/clang/docs/RealtimeSanitizer.html), 2025.
- RTSan is incompatible with AddressSanitizer, ThreadSanitizer, UndefinedBehaviorSanitizer, and MemorySanitizer in the owning Clang 20 driver source. Therefore `linux-rtsan` is a third nightly configuration, not an addition to either existing sanitizer list. [Clang 20.1.8 SanitizerArgs.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-20.1.8/clang/lib/Driver/SanitizerArgs.cpp), 2025.
- Concrete preset: select Clang 20.1.8 or newer plus compiler-rt; add `-fsanitize=realtime` to compile and link options for Duet targets and the render-test executable; retain debug symbols; run the callback-exercising CTest with the default `halt_on_error=true`. `-fno-omit-frame-pointer` is optional unless `fast_unwind_on_fatal=true` is selected. [Clang 20.1 RTSan runtime flags](https://releases.llvm.org/20.1.0/tools/clang/docs/RealtimeSanitizer.html#run-time-flags), 2025.

### Callback boundary and coverage

- The LLVM pass inserts `__rtsan_realtime_enter` and `__rtsan_realtime_exit` into the annotated function itself. Real-time state therefore begins when the uninstrumented virtual call lands in Duet; Tracktion and JUCE do not need RTSan instrumentation. [LLVM 20.1 RealtimeSanitizer pass](https://github.com/llvm/llvm-project/blob/llvmorg-20.1.0/llvm/lib/Transforms/Instrumentation/RealtimeSanitizer.cpp) and [runtime](https://github.com/llvm/llvm-project/blob/llvmorg-20.1.0/compiler-rt/lib/rtsan/rtsan.cpp), 2025.
- Clang permits an override to add the stronger nonblocking constraint. The portable declaration is conceptually `void processBlock(...) noexcept DUET_NONBLOCKING override`, with the macro feature-testing `__has_cpp_attribute(clang::nonblocking)`; repeat the attribute on each redeclaration. The same rule applies to a native `applyToBuffer` override. `nonblocking` includes nonallocation but does not imply C++ `noexcept`. [Clang 20.1 Attribute Reference](https://releases.llvm.org/20.1.0/tools/clang/docs/AttributeReference.html#performance-constraint-attributes), [Function Effect Analysis](https://releases.llvm.org/20.1.0/tools/clang/docs/FunctionEffectAnalysis.html), and [Language Extensions](https://releases.llvm.org/20.1.0/tools/clang/docs/LanguageExtensions.html), 2025.
- Runtime detection covers executed calls to functions instrumented from `[[clang::blocking]]` and an explicit POSIX interceptor set: allocation and mapping; file, descriptor, and stdio I/O; pthread locks, waits, creation, and joins; sleep and yield; sockets and name resolution; polling/event facilities; pipes; process creation and exec; and raw `syscall`. It cannot prove an unexecuted path or an unannotated custom blocking primitive safe. [LLVM 20.1 POSIX interceptors](https://github.com/llvm/llvm-project/blob/llvmorg-20.1.0/compiler-rt/lib/rtsan/rtsan_interceptors_posix.cpp), 2025.
- LLVM provides a scoped runtime disabler and function-name or call-stack suppressions, but no documented source-tree or module allowlist. A broad Tracktion/JUCE stack suppression would also hide violations called from Duet, so the standard forbids broad vendor-frame patterns. An exact upstream exception requires a documented false positive. [Clang 20.1 disabling and suppression documentation](https://releases.llvm.org/20.1.0/tools/clang/docs/RealtimeSanitizer.html#disabling-and-suppressing), 2025.
- `-Wfunction-effects` is a separate compile-time checker and is disabled by default. It can infer visible helpers but cross-translation-unit or indirect calls require annotations; precise pragma suppression exists for safe but unannotated library calls. It is useful incrementally, but it is not required for the runtime nightly and does not disable runtime interception. [Clang 20.1 Function Effect Analysis](https://releases.llvm.org/20.1.0/tools/clang/docs/FunctionEffectAnalysis.html), 2025.

### Headless render

- Tracktion offline rendering builds an Edit graph with plugins included and processes blocks through `NodeRenderContext` and `TracktionNodePlayer`. Native built-ins reach `Plugin::applyToBuffer`; hosted processors reach `AudioPluginInstance::processBlock`. [Renderer.cpp](https://github.com/Tracktion/tracktion_engine/blob/494e91d2ff546353b69723a5e992dd71d1a0204b/modules/tracktion_engine/model/export/tracktion_Renderer.cpp), [PluginNode.cpp](https://github.com/Tracktion/tracktion_engine/blob/494e91d2ff546353b69723a5e992dd71d1a0204b/modules/tracktion_engine/playback/graph/tracktion_PluginNode.cpp), [Plugin.cpp](https://github.com/Tracktion/tracktion_engine/blob/494e91d2ff546353b69723a5e992dd71d1a0204b/modules/tracktion_engine/plugins/tracktion_Plugin.cpp), and [ExternalPlugin.cpp](https://github.com/Tracktion/tracktion_engine/blob/494e91d2ff546353b69723a5e992dd71d1a0204b/modules/tracktion_engine/plugins/external/tracktion_ExternalPlugin.cpp), Tracktion commit 494e91d, 2026-08-03.
- No audio device is needed; render parameters supply sample rate and block size, and an active device play context during offline rendering is an error. JUCE message-system initialization is still required for graph setup and teardown. [NodeRenderContext.cpp](https://github.com/Tracktion/tracktion_engine/blob/494e91d2ff546353b69723a5e992dd71d1a0204b/modules/tracktion_engine/playback/graph/tracktion_NodeRenderContext.cpp), [Renderer.h](https://github.com/Tracktion/tracktion_engine/blob/494e91d2ff546353b69723a5e992dd71d1a0204b/modules/tracktion_engine/model/export/tracktion_Renderer.h), and [Tracktion TestRunner](https://github.com/Tracktion/tracktion_engine/blob/494e91d2ff546353b69723a5e992dd71d1a0204b/examples/TestRunner/TestRunner.h), same version.
- A non-vacuous test needs a positive render range, included track, enabled and initialized plugin, `usePlugins=true`, representative MIDI or audio input, and an assertion that the callback ran or the expected rendered feature exists. Disabled, bypassed, or initializing plugins may be omitted from the graph. The callback/feature assertion is the test-design inference that prevents such omission from looking like success. [Renderer.cpp](https://github.com/Tracktion/tracktion_engine/blob/494e91d2ff546353b69723a5e992dd71d1a0204b/modules/tracktion_engine/model/export/tracktion_Renderer.cpp) and [EditNodeBuilder.cpp](https://github.com/Tracktion/tracktion_engine/blob/494e91d2ff546353b69723a5e992dd71d1a0204b/modules/tracktion_engine/playback/graph/tracktion_EditNodeBuilder.cpp), same version.

### Coding-standard deliverable

`docs/CODING_STANDARDS.md` now defines the callback boundary and bans allocation/deallocation, locks and waits, operating-system I/O, project-model/UI access, and unbounded cross-thread communication. It requires prepared memory, bounded work, always-lock-free atomics, explicit queue overflow behavior, `noexcept` plus the portable nonblocking annotation, offline-render RTSan coverage, and narrow documented suppression policy.

## Unresolved

- The repository contains no CMake implementation or audio processors yet, so an end-to-end Clang 20 RTSan render could not be built in this session. The first implementation slice must prove the preset by injecting one known allocation into each callback seam, observing a failing test, then removing it and observing a pass.
- Compile-time `-Wfunction-effects` across JUCE-facing callback code may require precise annotations or warning escapes for safe unannotated APIs. This does not affect the runtime RTSan verdict; adopt it incrementally only after measuring its diagnostics.

## Sources

- LLVM/Clang 20.1 release notes, RTSan documentation, Attribute Reference, Function Effect Analysis, Language Extensions, sanitizer driver, instrumentation pass, runtime, supported-architecture definitions, and POSIX interceptor source.
- JUCE `AudioProcessor` primary API documentation, accessed 2026-08-10.
- Tracktion Engine commit 494e91d source: renderer, graph player, plugin node, native plugin, external plugin, async utilities, and TestRunner.
- Local Ubuntu Clang 18.1.3 flag probe, 2026-08-10.
- `docs/CODING_STANDARDS.md`, updated by this session.
