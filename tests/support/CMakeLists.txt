# duet_test_support — helpers the suites share: a temporary project folder, the
# message-loop pump, the headless play retry, and audio-file measurement.
#
# It links the engine PRIVATE, exactly as the facades do, so its public header
# stays engine-free and the suites that use it do too.
add_library(duet_test_support STATIC
    src/TestSupport.cpp)
add_library(duet::test_support ALIAS duet_test_support)

target_include_directories(duet_test_support PUBLIC include)
target_compile_features(duet_test_support PRIVATE cxx_std_20)
target_link_libraries(duet_test_support
    PUBLIC duet::model duet::persistence
    PRIVATE duet_engine_config)
