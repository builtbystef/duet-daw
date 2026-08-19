# Duet's own tests. Catch2 v3, headless.
#
# duet_tests links the two facades and the test-support library, and no engine
# target, so it also enforces the engine-free rule: a public facade header that
# reached for an engine or JUCE header would fail to compile here.
add_subdirectory(support)

add_executable(duet_tests
    AutomationOpsTests.cpp
    ClipOpsTests.cpp
    DemoWalkthroughTests.cpp
    EditVocabularyTests.cpp
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
    duet::model
    duet::persistence
    duet::test_support
    Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(duet_tests)
