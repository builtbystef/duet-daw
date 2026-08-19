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

# A compiler cache, when the machine has one. Every Duet translation unit pulls
# the whole JUCE and Tracktion header set, so recompiling one that nothing has
# changed costs the same as compiling it the first time; ccache turns that into
# a hash lookup. Set here rather than in a preset because it must hold for the
# vendored trees too, and it is deliberately optional — a machine without ccache
# builds exactly as before.
#
# CI turns the option off rather than relying on the runner not having ccache:
# spec b1j3me settled that CI builds cold, and a cache that appears in a future
# runner image would quietly undo that.
option(DUET_CCACHE_ENABLED "Compile through ccache when the machine has one" ON)

if(DUET_CCACHE_ENABLED)
    find_program(DUET_CCACHE ccache)
    if(DUET_CCACHE)
        set(CMAKE_C_COMPILER_LAUNCHER "${DUET_CCACHE}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${DUET_CCACHE}")
        message(STATUS "Duet: compiling through ccache (${DUET_CCACHE})")
    endif()
endif()

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

# Every Duet target that compiles JUCE or engine sources links one of these two,
# so that all of them see one identical configuration. JUCE modules are INTERFACE
# sources: they compile into each consuming target, and a definition that differs
# between two targets would be an ODR violation rather than a build error.
#
# The split is by which modules a definition reaches. duet_juce_config carries
# what juce_core, juce_graphics and juce_gui_basics read, so the GUI layer — which
# compiles those and no engine source — can share it. duet_engine_config adds what
# only the engine and the audio modules read, and every engine target links it.
add_library(duet_juce_config INTERFACE)
target_compile_definitions(duet_juce_config INTERFACE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    # Headless tests drive the engine's deferred work — the async clip re-sort
    # above all — by running the message loop for a fixed time, and JUCE gates
    # its only timed pump behind this flag. It permits the API; it forces no
    # modal behaviour on the app.
    JUCE_MODAL_LOOPS_PERMITTED=1
)
target_link_libraries(duet_juce_config INTERFACE
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

add_library(duet_engine_config INTERFACE)
target_compile_definitions(duet_engine_config INTERFACE
    JUCE_JACK=1                                   # the Linux path is JACK via PipeWire
    TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH=1    # without a stretch backend, time-stretch is off entirely
    JUCE_PLUGINHOST_LADSPA=0
)
target_link_libraries(duet_engine_config INTERFACE
    duet_juce_config
    tracktion::tracktion_engine
    juce::juce_audio_utils
    # GCC lowers Tracktion's 16-byte std::atomic (WaveNode) to libatomic calls and
    # nothing upstream links it.
    atomic
)

add_subdirectory(modules/duet_model)
add_subdirectory(modules/duet_persistence)
add_subdirectory(modules/duet_gui)
add_subdirectory(modules/duet_app)

include(CTest)
add_subdirectory(tests)
