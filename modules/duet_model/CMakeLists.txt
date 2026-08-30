# duet_model — the edit vocabulary layer over the engine's Edit, and the engine seam.
#
# The engine-free rule: this target links duet_engine_config PRIVATE, so no engine
# or JUCE include directory reaches a consumer. A public header that included one
# would stop compiling in the tests, which link this target and nothing else.
add_library(duet_model STATIC
    src/AppSettings.cpp
    src/AudioFile.cpp
    src/Devices.cpp
    src/EditOps.cpp
    src/PluginScan.cpp
    src/Recording.cpp
    src/Session.cpp
    src/Suggestion.cpp)
add_library(duet::model ALIAS duet_model)

target_include_directories(duet_model PUBLIC include)
target_compile_features(duet_model PRIVATE cxx_std_20)
target_link_libraries(duet_model PRIVATE duet_engine_config)
target_include_directories(duet_model PRIVATE engine)

# The narrow back door through the engine seam. Persistence snapshots the Edit;
# the component-only plugin editor bridge hosts the engine's AudioProcessor
# editor (ADR 0008). The header lives outside the public include path, and this
# target is the explicit, greppable opt-in.
add_library(duet_model_engine_access INTERFACE)
add_library(duet::model_engine_access ALIAS duet_model_engine_access)

target_include_directories(duet_model_engine_access INTERFACE engine)
target_link_libraries(duet_model_engine_access INTERFACE duet_model)
