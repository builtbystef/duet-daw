# PROTOTYPE — undo-vocabulary spike for roadmap node skb4tp. Disposable; never ship.
# Toolchain recipe from node psmj4y, as proven by the walking skeleton (ddp1qt).
cmake_minimum_required(VERSION 3.22)
project(DuetUndoVocabulary VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# JUCE must be declared BEFORE Tracktion so Tracktion's
# "if(NOT TARGET juce::juce_core)" guard skips its vendored 8.0.13 submodule.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        f8f8864172464b9adf9eba6101e1f784838d1597 # tag 9.0.0
)

set(TE_ADD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tracktion
    GIT_REPOSITORY https://github.com/Tracktion/tracktion_engine.git
    GIT_TAG        494e91d2ff546353b69723a5e992dd71d1a0204b # develop 2026-08-03
    GIT_SUBMODULES ""  # its only submodule is JUCE over SSH; we supply JUCE 9 ourselves
)

FetchContent_MakeAvailable(JUCE tracktion)

juce_add_console_app(duet_undo_spike
    PRODUCT_NAME "Duet Undo Vocabulary PROTOTYPE")

target_sources(duet_undo_spike PRIVATE
    src/Main.cpp
    src/DuetEdit.cpp)

# Nothing in the dependency graph enforces C++20 — set it by hand (psmj4y).
target_compile_features(duet_undo_spike PRIVATE cxx_std_20)

target_compile_definitions(duet_undo_spike PRIVATE
    JUCE_JACK=1
    JUCE_PLUGINHOST_VST3=1
    TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_MODAL_LOOPS_PERMITTED=1  # the scenario runner pumps the message loop with runDispatchLoopUntil
)

target_link_libraries(duet_undo_spike PRIVATE
    atomic  # GCC needs libatomic for Tracktion's 16-byte std::atomic (WaveNode) — psmj4y
    tracktion::tracktion_engine
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
)

# Interactive UI over the same vocabulary layer, for the user's verdict session.
juce_add_gui_app(duet_undo_spike_gui
    PRODUCT_NAME "Duet Undo Vocabulary GUI PROTOTYPE")

target_sources(duet_undo_spike_gui PRIVATE
    src/GuiMain.cpp
    src/DuetEdit.cpp)

target_compile_features(duet_undo_spike_gui PRIVATE cxx_std_20)

target_compile_definitions(duet_undo_spike_gui PRIVATE
    JUCE_JACK=1
    JUCE_PLUGINHOST_VST3=1
    TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:duet_undo_spike_gui,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="0.0.1"
)

target_link_libraries(duet_undo_spike_gui PRIVATE
    atomic
    tracktion::tracktion_engine
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
)
