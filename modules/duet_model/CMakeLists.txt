# duet_model — the edit vocabulary layer over the engine's Edit, and the engine seam.
#
# The engine-free rule: this target links duet_engine_config PRIVATE, so no engine
# or JUCE include directory reaches a consumer. A public header that included one
# would stop compiling in the tests, which link this target and nothing else.
add_library(duet_model STATIC
    src/EditOps.cpp
    src/Session.cpp)
add_library(duet::model ALIAS duet_model)

target_include_directories(duet_model PUBLIC include)
target_compile_features(duet_model PRIVATE cxx_std_20)
target_link_libraries(duet_model PRIVATE duet_engine_config)
target_include_directories(duet_model PRIVATE engine)

# The back door through the engine seam. duet_persistence writes the project's
# whole state, and the engine's state tree is that state, so it needs the Edit
# itself. The header lives outside the public include path, and this target is
# how a module says it needs it — one line to grep for, and duet_persistence is
# the only place it appears.
add_library(duet_model_engine_access INTERFACE)
add_library(duet::model_engine_access ALIAS duet_model_engine_access)

target_include_directories(duet_model_engine_access INTERFACE engine)
target_link_libraries(duet_model_engine_access INTERFACE duet_model)
