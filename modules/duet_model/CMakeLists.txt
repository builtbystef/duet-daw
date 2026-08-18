# duet_model — the edit vocabulary layer over the engine's Edit, and the engine seam.
#
# The engine-free rule: this target links duet_engine_config PRIVATE, so no engine
# or JUCE include directory reaches a consumer. A public header that included one
# would stop compiling in the tests, which link this target and nothing else.
add_library(duet_model STATIC
    src/Session.cpp)
add_library(duet::model ALIAS duet_model)

target_include_directories(duet_model PUBLIC include)
target_compile_features(duet_model PRIVATE cxx_std_20)
target_link_libraries(duet_model PRIVATE duet_engine_config)
