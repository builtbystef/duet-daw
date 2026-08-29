# The interface's own suite, and the one place a Duet test links JUCE.
#
# duet_tests deliberately links no graphical target, which is what keeps the
# facades engine-free and the view-models paintless. What lives here is what a
# paintless test cannot ask: measurements a window alone knows — that Inter is
# inside the binary, that its digits are tabular, that a scrollbar is thin — and
# what the window's components do with the view-models beneath them. Never what
# any of it looks like: paint stays untested (spec 535bbo).
add_executable(duet_gui_tests
    GuiSettingsHome.cpp
    BrowserCanvasTests.cpp
    CollaboratorPanelCanvasTests.cpp
    GraphiteLookTests.cpp
    MainShellTests.cpp
    TextTests.cpp
    TypographyTests.cpp)

target_compile_features(duet_gui_tests PRIVATE cxx_std_20)
target_link_libraries(duet_gui_tests PRIVATE
    duet::gui_components
    Catch2::Catch2WithMain)

catch_discover_tests(duet_gui_tests)
