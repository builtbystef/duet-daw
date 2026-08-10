# PROTOTYPE — milestone-one UI for roadmap node r4m858. Disposable; never ship.
# GUI-only: pure JUCE, no engine. Answers look-and-feel questions from grill s11o4w.
cmake_minimum_required(VERSION 3.22)
project(DuetUiPrototype VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        f8f8864172464b9adf9eba6101e1f784838d1597 # tag 9.0.0 — same pin as the walking skeleton
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(duet_ui_prototype
    PRODUCT_NAME "Duet UI PROTOTYPE")

target_sources(duet_ui_prototype PRIVATE src/Main.cpp)

target_compile_features(duet_ui_prototype PRIVATE cxx_std_20)

target_compile_definitions(duet_ui_prototype PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:duet_ui_prototype,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="0.0.1"
)

target_link_libraries(duet_ui_prototype PRIVATE
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
)
