# duet_app — the JUCE GUI application shell: message loop, device management,
# transport, and the host of duet_gui's surfaces.
#
# Project lifecycle is paintless so launch, whole-folder transitions and dirty
# decisions are exercised without a window or dialog. The app-global settings
# store is here for the same reason: it needs no window, and what the tests must
# be able to link is the store the shell actually runs on.
# The Collaborator lives here for the same reason: it is where the panel and the
# service meet, and neither module can name the other — one links no JUCE and
# the other no engine. It reaches the message thread through the two functions
# its host supplies, so it links no JUCE either and the tests drive it headless.
add_library(duet_app_core STATIC
    src/Collaborator.cpp
    src/MessageThreadMarshal.cpp
    src/ModelAccess.cpp
    src/OpeningContext.cpp
    src/ProjectLifecycle.cpp
    src/PropertyStorageSettings.cpp
    src/SuggestionSurfaces.cpp)
add_library(duet::app_core ALIAS duet_app_core)

target_include_directories(duet_app_core PUBLIC include)
target_compile_features(duet_app_core PRIVATE cxx_std_20)
# duet::collab_tools is PUBLIC because the Collaborator's own header names the
# service, the tools and the ledger; the model comes with it, that target being
# what reads the project.
target_link_libraries(duet_app_core PUBLIC duet::gui duet::persistence duet::collab_tools)
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

# The transcription model and the runtime that reads it ship the same way, and
# for the same reason: the analysis layer looks beside the running executable
# first, so an installed Duet finds its weights without being told where they
# are. Copied rather than referenced, so that the licence notice
# THIRD_PARTY_NOTICES.md carries is about a file that is actually there.
if(DUET_ENABLE_POLYPHONIC_TRANSCRIPTION)
    add_custom_command(TARGET duet_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DUET_BASIC_PITCH_MODEL}" "$<TARGET_FILE_DIR:duet_app>/nmp.onnx"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DUET_ONNXRUNTIME_LIBRARY}" "$<TARGET_FILE_DIR:duet_app>/libonnxruntime.so.1"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
                "$<TARGET_FILE_DIR:duet_app>/THIRD_PARTY_NOTICES.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/licences/Apache-2.0.txt"
                "$<TARGET_FILE_DIR:duet_app>/licences/Apache-2.0.txt"
        COMMENT "Putting the transcription model, its runtime and their notices beside the application"
        VERBATIM)
endif()

# The sidecar ships inside the DAW install, beside the application, which is
# where the Collaborator looks for it (ADR 0003).
if(TARGET duet_sidecar)
    add_dependencies(duet_app duet_sidecar)
    add_custom_command(TARGET duet_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DUET_SIDECAR_BINARY}" "$<TARGET_FILE_DIR:duet_app>/duet-sidecar"
        COMMENT "Putting the sidecar beside the application"
        VERBATIM)
endif()
