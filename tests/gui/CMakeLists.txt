# The interface's own suite, and the one place a Duet test links JUCE.
#
# duet_tests deliberately links no graphical target, which is what keeps the
# facades engine-free and the view-models paintless. The few things this slice
# has to assert are measurements a window cannot be asked for — that Inter is
# inside the binary, that its digits are tabular, that a scrollbar is thin — so
# they live here, against duet::gui_components, and never against paint.
add_executable(duet_gui_tests
    GraphiteLookTests.cpp
    MainShellTests.cpp
    TypographyTests.cpp)

target_compile_features(duet_gui_tests PRIVATE cxx_std_20)
target_link_libraries(duet_gui_tests PRIVATE
    duet::gui_components
    Catch2::Catch2WithMain)

catch_discover_tests(duet_gui_tests)
