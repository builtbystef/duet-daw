# duet_persistence — project folder lifecycle, snapshot save, autosave, migration.
# Same engine-free facade rule as duet_model; this module needs no engine at all yet.
add_library(duet_persistence STATIC
    src/ProjectLayout.cpp)
add_library(duet::persistence ALIAS duet_persistence)

target_include_directories(duet_persistence PUBLIC include)
target_compile_features(duet_persistence PRIVATE cxx_std_20)
