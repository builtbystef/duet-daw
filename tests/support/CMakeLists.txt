# duet_test_support — helpers the suites share: a temporary project folder, the
# message-loop pump, the headless play retry, and the audio-correctness harness
# — offline renders and the feature measurements ADR 0006 asserts them on.
#
# It links the engine PRIVATE, exactly as the facades do, so its public headers
# stay engine-free and the suites that use them do too.
add_library(duet_test_support STATIC
    src/RenderHarness.cpp
    src/TestSupport.cpp)
add_library(duet::test_support ALIAS duet_test_support)

target_include_directories(duet_test_support PUBLIC include)
target_compile_features(duet_test_support PRIVATE cxx_std_20)
target_link_libraries(duet_test_support
    PUBLIC duet::gui duet::model duet::persistence
    PRIVATE duet_engine_config)
