# duet_app — the JUCE GUI application shell: message loop, device management,
# transport, and the host of duet_gui's surfaces.
#
# Project lifecycle is paintless so launch, whole-folder transitions and dirty
# decisions are exercised without a window or dialog. The app-global settings
# store is here for the same reason: it needs no window, and what the tests must
# be able to link is the store the shell actually runs on.
add_library(duet_app_core STATIC
    src/ProjectLifecycle.cpp
    src/PropertyStorageSettings.cpp)
add_library(duet::app_core ALIAS duet_app_core)

target_include_directories(duet_app_core PUBLIC include)
target_compile_features(duet_app_core PRIVATE cxx_std_20)
target_link_libraries(duet_app_core PUBLIC duet::gui duet::persistence)
# The settings store is the model's app-global one, reached through the facade,
# so no engine include directory reaches a consumer of this target.
target_link_libraries(duet_app_core PRIVATE duet::model)

juce_add_gui_app(duet_app
    PRODUCT_NAME "Duet"
    VERSION      ${PROJECT_VERSION})

target_sources(duet_app PRIVATE
    src/Main.cpp)

target_compile_features(duet_app PRIVATE cxx_std_20)
target_compile_definitions(duet_app PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:duet_app,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:duet_app,JUCE_VERSION>")

target_link_libraries(duet_app PRIVATE
    duet::app_core
    duet::gui_components
    duet::model
    duet::persistence
    duet_engine_config
    juce::juce_gui_extra)

# The sidecar ships inside the DAW install, beside the application, which is
# where a Collaborator wired into the shell will look for it (ADR 0003). Nothing
# spawns it from there yet — the panel meets the real service at 7tw2tz — but the
# layout the spec asks for is the build's to produce and not that issue's.
if(TARGET duet_sidecar)
    add_dependencies(duet_app duet_sidecar)
    add_custom_command(TARGET duet_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DUET_SIDECAR_BINARY}" "$<TARGET_FILE_DIR:duet_app>/duet-sidecar"
        COMMENT "Putting the sidecar beside the application"
        VERBATIM)
endif()
