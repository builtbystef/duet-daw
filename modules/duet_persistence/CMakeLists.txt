# duet_persistence — project folder lifecycle, snapshot save, autosave, migration.
#
# Same engine-free facade rule as duet_model: the engine comes in PRIVATE, so no
# engine or JUCE include directory reaches a consumer. duet::model_engine_access
# is the one back door through the engine seam — persistence writes the project's
# whole state, and the engine's state tree is that state.
add_library(duet_persistence STATIC
    src/Project.cpp
    src/ProjectLayout.cpp)
add_library(duet::persistence ALIAS duet_persistence)

target_include_directories(duet_persistence PUBLIC include)
target_compile_features(duet_persistence PRIVATE cxx_std_20)
target_link_libraries(duet_persistence
    PUBLIC duet::model
    PRIVATE duet::model_engine_access duet_engine_config)
