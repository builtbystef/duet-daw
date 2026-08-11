# PROTOTYPE — offline-render correctness spike for roadmap node xciphe.
# Disposable; never ship.
cmake_minimum_required(VERSION 3.22)
project(DuetOfflineRenderPrototype VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        f8f8864172464b9adf9eba6101e1f784838d1597 # tag 9.0.0
)

set(TE_ADD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tracktion
    GIT_REPOSITORY https://github.com/Tracktion/tracktion_engine.git
    GIT_TAG        494e91d2ff546353b69723a5e992dd71d1a0204b # develop 2026-08-03
    GIT_SUBMODULES ""
)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        8b08d4d79514f45f7e4ce2a607ac9c94e920d1bb # tag 3.15.3
)

FetchContent_MakeAvailable(JUCE tracktion Catch2)

add_executable(duet_offline_render_spike)

target_sources(duet_offline_render_spike PRIVATE
    src/OfflineRenderPrototype.cpp
    src/OfflineRenderPrototype.h
    src/Tests.cpp)

target_compile_features(duet_offline_render_spike PRIVATE cxx_std_20)

target_compile_definitions(duet_offline_render_spike PRIVATE
    JUCE_JACK=1
    JUCE_PLUGINHOST_VST3=1
    TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_PLUGINHOST_LADSPA=0
)

target_link_libraries(duet_offline_render_spike PRIVATE
    atomic
    Catch2::Catch2WithMain
    tracktion::tracktion_engine
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
)

include(CTest)
# PROTOTYPE trap: Catch2 3.15 discovery writes its JSON listing below
# XDG_RUNTIME_DIR (/run/user/1000 here), which this sandbox exposes read-only.
# A single explicit CTest entry still runs the complete Catch2 executable.
add_test(NAME offline-render-prototype COMMAND duet_offline_render_spike)
