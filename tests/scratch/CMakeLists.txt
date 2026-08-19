# Disposable engine probes. A short program that opens a Session and asks the
# engine a question has repeatedly been cheaper than reading vendored sources,
# and this target is where that program lives. It links the same libraries
# duet_tests does and is excluded from the ordinary build and from ctest, so a
# probe that stops compiling is not a broken build. Build it with
# `--target duet_scratch`.
add_executable(duet_scratch main.cpp)

target_compile_features(duet_scratch PRIVATE cxx_std_20)
target_link_libraries(duet_scratch PRIVATE
    duet::model
    duet::persistence
    duet::test_support)
