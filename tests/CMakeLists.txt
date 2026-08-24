# Duet's own tests. Catch2 v3, headless.
#
# duet_tests links the two facades, the paintless half of duet_gui, the
# Collaborator service, and the test-support library — and no engine target and
# nothing graphical, so it also enforces two rules by construction: a public
# facade header that reached for an engine or JUCE header would fail to compile
# here, and so would a view-model header that reached for a component or a paint
# type. The interface's own JUCE-linked suite is in gui/.
add_subdirectory(support)
add_subdirectory(sidecar_double)
add_subdirectory(vst3_fixtures)

add_executable(duet_tests
    TestMain.cpp
    AppearanceTests.cpp
    ArrangementTrackTests.cpp
    ArrangementViewTests.cpp
    AutomationOpsTests.cpp
    AutosaveTests.cpp
    ClipOpsTests.cpp
    CollaboratorServiceTests.cpp
    DemoWalkthroughTests.cpp
    DeviceRebuildTests.cpp
    EditVocabularyTests.cpp
    GraphiteTokenTests.cpp
    MidiOpsTests.cpp
    MixerOpsTests.cpp
    PlaybackLevelTests.cpp
    PluginHostingTests.cpp
    PluginOpsTests.cpp
    ProjectLayoutTests.cpp
    ProjectAndTransportOpsTests.cpp
    ProjectTests.cpp
    ProjectViewTests.cpp
    RecordingTests.cpp
    RenderingTests.cpp
    SessionTests.cpp
    ShortcutTests.cpp
    SuggestionTests.cpp
    TimelineGeometryTests.cpp
    TrackOpsTests.cpp
    ViewStateTests.cpp
    WindowGeometryTests.cpp)

target_compile_features(duet_tests PRIVATE cxx_std_20)
target_link_libraries(duet_tests PRIVATE
    duet::collab
    duet::gui
    duet::model
    duet::persistence
    duet::test_support
    Catch2::Catch2)

# The suite spawns the test-double sidecar, so it needs it built and needs to
# know where it landed. It scans the two VST3 fixtures from their bundles for
# the same reason. Generator expressions are the only honest paths: each one
# carries the configuration's name.
add_dependencies(duet_tests
    duet_sidecar_double
    duet_good_vst3_fixture_VST3
    duet_crashing_vst3_fixture_VST3)
target_compile_definitions(duet_tests PRIVATE
    DUET_SIDECAR_DOUBLE="$<TARGET_FILE:duet_sidecar_double>"
    DUET_GOOD_VST3_FIXTURE="$<GENEX_EVAL:$<TARGET_PROPERTY:duet_good_vst3_fixture_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>"
    DUET_CRASHING_VST3_FIXTURE="$<GENEX_EVAL:$<TARGET_PROPERTY:duet_crashing_vst3_fixture_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>")

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(duet_tests)

add_subdirectory(gui)
add_subdirectory(scratch EXCLUDE_FROM_ALL)
