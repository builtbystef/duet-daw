function(duet_add_vst3_fixture target product_name plugin_code source)
    juce_add_plugin(${target}
        COMPANY_NAME             "Duet"
        BUNDLE_ID                "works.earendil.duet.${target}"
        IS_SYNTH                 FALSE
        NEEDS_MIDI_INPUT         FALSE
        NEEDS_MIDI_OUTPUT        FALSE
        IS_MIDI_EFFECT           FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD  FALSE
        VST3_AUTO_MANIFEST       FALSE
        PLUGIN_MANUFACTURER_CODE Duet
        PLUGIN_CODE              ${plugin_code}
        FORMATS                  VST3
        PRODUCT_NAME             "${product_name}")

    target_sources(${target} PRIVATE ${source})
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(${target} PRIVATE
        duet_juce_config
        juce::juce_audio_processors)
endfunction()

duet_add_vst3_fixture(
    duet_good_vst3_fixture
    "Duet Good VST3 Fixture"
    DgFx
    GoodPlugin.cpp)

duet_add_vst3_fixture(
    duet_crashing_vst3_fixture
    "Duet Crashing VST3 Fixture"
    DcFx
    CrashingPlugin.cpp)

duet_add_vst3_fixture(
    duet_raising_vst3_fixture
    "Duet Raising VST3 Fixture"
    DrFx
    RaisingPlugin.cpp)
