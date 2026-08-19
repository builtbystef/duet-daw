# Duet's own tests. Catch2 v3, headless.
#
# duet_tests links the two facades, the paintless half of duet_gui, and the
# test-support library — and no engine target and nothing graphical, so it also
# enforces two rules by construction: a public facade header that reached for an
# engine or JUCE header would fail to compile here, and so would a view-model
# header that reached for a component or a paint type. The interface's own
# JUCE-linked suite is in gui/.
add_subdirectory(support)

add_executable(duet_tests
    AppearanceTests.cpp
    AutomationOpsTests.cpp
    ClipOpsTests.cpp
    DemoWalkthroughTests.cpp
    DeviceRebuildTests.cpp
    EditVocabularyTests.cpp
    GraphiteTokenTests.cpp
    MidiOpsTests.cpp
    MixerOpsTests.cpp
    PlaybackLevelTests.cpp
    PluginOpsTests.cpp
    ProjectLayoutTests.cpp
    ProjectAndTransportOpsTests.cpp
    ProjectTests.cpp
    RecordingTests.cpp
    SessionTests.cpp
    TrackOpsTests.cpp)

target_compile_features(duet_tests PRIVATE cxx_std_20)
target_link_libraries(duet_tests PRIVATE
    duet::gui
    duet::model
    duet::persistence
    duet::test_support
    Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(duet_tests)

add_subdirectory(gui)
add_subdirectory(scratch EXCLUDE_FROM_ALL)
