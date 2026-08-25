# The real-time probes: the smallest Duet audio callback at each of the two
# seams the Real-time audio standard names, and what the linux-rtsan nightly
# runs (ADR 0006, issue uaqfnv).
#
# Both link duet::realtime, which is what carries the nonblocking annotation and
# — in the rtsan configuration and nowhere else — the sanitizer itself. That
# link is the declaration that a target holds callback code, so the sanitizer's
# reach is a fact about the build graph rather than a list in a preset.

# The engine-native probe, in a library the suite links. Its public header is
# engine-free, so duet_tests still links no engine target; the probe reaches the
# Edit through the same explicit back door persistence uses.
add_library(duet_rt_probes STATIC NativeRealtimeProbe.cpp)
add_library(duet::rt_probes ALIAS duet_rt_probes)

target_include_directories(duet_rt_probes PUBLIC .)
target_compile_features(duet_rt_probes PRIVATE cxx_std_20)
target_link_libraries(duet_rt_probes
    PUBLIC duet::model duet::realtime
    PRIVATE duet::model_engine_access duet_engine_config)

# The JUCE-hosted probe, as a VST3, because that is the only way the engine
# hosts a juce::AudioProcessor. It is built and scanned the way the fixtures in
# tests/vst3_fixtures are, and it is deliberately not one of them: those stand
# in for a producer's third-party plugin, and this is Duet's own callback under
# the sanitizer.
juce_add_plugin(duet_realtime_probe_vst3
    COMPANY_NAME             "Duet"
    BUNDLE_ID                "works.earendil.duet.realtime_probe"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    VST3_AUTO_MANIFEST       FALSE
    PLUGIN_MANUFACTURER_CODE Duet
    PLUGIN_CODE              DrPr
    FORMATS                  VST3
    PRODUCT_NAME             "Duet Realtime Probe VST3")

target_sources(duet_realtime_probe_vst3 PRIVATE JuceRealtimeProbe.cpp)
target_compile_features(duet_realtime_probe_vst3 PRIVATE cxx_std_20)
target_compile_definitions(duet_realtime_probe_vst3 PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(duet_realtime_probe_vst3 PRIVATE
    duet_juce_config
    duet::realtime
    juce::juce_audio_processors)

# A sanitizer-instrumented shared library does not carry the runtime: Clang
# links it into executables only, and leaves the enter/exit calls undefined in a
# module for the host that loads it to resolve — which is exactly what happens
# here, the runtime being in duet_tests and the bundle being opened into it.
# JUCE ends a plugin's link with -Wl,--no-undefined, so say the opposite after
# it, and only in the configuration that instruments anything.
if(DUET_RTSAN)
    target_link_options(duet_realtime_probe_vst3_VST3 PRIVATE -Wl,-z,undefs)
endif()
