# duet_app — the JUCE GUI application shell: message loop, device management,
# transport, and the host of duet_gui's surfaces.
juce_add_gui_app(duet_app
    PRODUCT_NAME "Duet"
    VERSION      ${PROJECT_VERSION})

target_sources(duet_app PRIVATE
    src/Main.cpp
    src/PropertyStorageSettings.cpp)

target_compile_features(duet_app PRIVATE cxx_std_20)
target_compile_definitions(duet_app PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:duet_app,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:duet_app,JUCE_VERSION>")

target_link_libraries(duet_app PRIVATE
    duet::gui_components
    duet::model
    duet::persistence
    duet_engine_config
    juce::juce_gui_extra)
