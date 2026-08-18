# Duet DAW — root build.
#
# The toolchain is settled at roadmap node psmj4y and corrected by the walking
# skeleton ddp1qt; spec b1j3me records it. Two corrections are load-bearing and
# easy to lose, so they are marked where they apply: project() must declare
# VERSION, and GCC needs libatomic linked by hand.
cmake_minimum_required(VERSION 3.22)

# VERSION is mandatory: juce_add_gui_app hard-errors without a project version.
# LANGUAGES must list C: JUCE hard-errors without a C compiler.
project(Duet VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# JUCE is declared before Tracktion Engine on purpose. Tracktion guards its own
# JUCE acquisition with `if(NOT TARGET juce::juce_core)`, so declaring JUCE first
# makes it skip its vendored 8.0.13 submodule — which is also an SSH URL that
# breaks anonymous clones.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        f8f8864172464b9adf9eba6101e1f784838d1597 # tag 9.0.0
)

# TE_ADD_EXAMPLES is Tracktion's only option and defaults ON; the examples pull in
# a DemoRunner, benchmarks and a test runner Duet does not build.
set(TE_ADD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tracktion
    GIT_REPOSITORY https://github.com/Tracktion/tracktion_engine.git
    GIT_TAG        494e91d2ff546353b69723a5e992dd71d1a0204b # develop, 2026-08-03
    GIT_SUBMODULES ""  # its only submodule is JUCE over SSH; we supply JUCE ourselves
)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        8b08d4d79514f45f7e4ce2a607ac9c94e920d1bb # tag v3.15.3
)

FetchContent_MakeAvailable(JUCE tracktion Catch2)

# Every Duet target that compiles JUCE or engine sources links this, so that all
# of them see one identical configuration. JUCE modules are INTERFACE sources:
# they compile into each consuming target, and a definition that differs between
# two targets would be an ODR violation rather than a build error.
add_library(duet_engine_config INTERFACE)
target_compile_definitions(duet_engine_config INTERFACE
    JUCE_JACK=1                                   # the Linux path is JACK via PipeWire
    TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH=1    # without a stretch backend, time-stretch is off entirely
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_PLUGINHOST_LADSPA=0
    # Headless tests drive the engine's deferred work — the async clip re-sort
    # above all — by running the message loop for a fixed time, and JUCE gates
    # its only timed pump behind this flag. It permits the API; it forces no
    # modal behaviour on the app.
    JUCE_MODAL_LOOPS_PERMITTED=1
)
target_link_libraries(duet_engine_config INTERFACE
    tracktion::tracktion_engine
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
    # GCC lowers Tracktion's 16-byte std::atomic (WaveNode) to libatomic calls and
    # nothing upstream links it.
    atomic
)

add_subdirectory(modules/duet_model)
add_subdirectory(modules/duet_persistence)
add_subdirectory(modules/duet_app)

include(CTest)
add_subdirectory(tests)
