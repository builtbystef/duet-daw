# duet_gui — the producer-facing interface (spec 535bbo).
#
# The module is two targets because the seam every surface splits along is worth
# a link error rather than a review comment. duet::gui is the paintless half —
# view-models, the token set, the settings interface — and it links no JUCE at
# all, so a view-model header that reached for a component or a paint type would
# stop compiling in duet_tests, which links this target and nothing graphical.
# duet::gui_components is the thin half: the JUCE components that paint the
# view-models' state and forward input to them.
add_library(duet_gui STATIC
    src/Appearance.cpp
    src/ArrangementView.cpp
    src/AudioMidiSettings.cpp
    src/AutomationLanes.cpp
    src/Browser.cpp
    src/CollaboratorPanel.cpp
    src/AutosaveSettings.cpp
    src/Export.cpp
    src/Mixer.cpp
    src/ModelPicker.cpp
    src/PianoRoll.cpp
    src/PluginPresets.cpp
    src/PluginScan.cpp
    src/Rendering.cpp
    src/ProjectsSettings.cpp
    src/Selection.cpp
    src/SessionClock.cpp
    src/Shortcuts.cpp
    src/Snap.cpp
    src/Suggestions.cpp
    src/TimelineGeometry.cpp
    src/TransportBar.cpp
    src/Tokens.cpp
    src/ViewState.cpp
    src/WindowGeometry.cpp)
add_library(duet::gui ALIAS duet_gui)

target_include_directories(duet_gui PUBLIC include)
target_compile_features(duet_gui PRIVATE cxx_std_20)
target_link_libraries(duet_gui PUBLIC duet::model duet::persistence)

# What a development build is, said once for everything that reads the panel's
# header. The Collaborator's raw tool-call trace is a developer's window into a
# run and nobody else's (spec js437t), so a Release build — the shipping path —
# keeps none of it. PUBLIC because the answer is a constant in a public header,
# and two consumers that disagreed about it would be an ODR violation rather
# than a build error.
target_compile_definitions(duet_gui PUBLIC $<$<CONFIG:Debug>:DUET_DEVELOPMENT_BUILD=1>)

# Inter (SIL OFL 1.1), the application typeface, compiled into the binary rather
# than looked for on the machine: the Graphite look reads the same on a machine
# that has never heard of Inter. The licence travels beside the font files.
juce_add_binary_data(duet_gui_fonts
    HEADER_NAME DuetFonts.h
    NAMESPACE   DuetFonts
    SOURCES
        assets/fonts/Inter-Regular.ttf
        assets/fonts/Inter-Bold.ttf)

# juce_add_binary_data makes a library of raw arrays, and without this it cannot
# be linked into anything built as position-independent code.
#
# Its header is generated, and generated code is not Duet's to lint: SYSTEM is
# how a consumer says so, and it is what keeps the lint sweep off a file nobody
# writes. The generated path sits under build/modules/duet_gui, which the lint's
# header filter would otherwise take for one of ours.
set_target_properties(duet_gui_fonts PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
        "$<TARGET_PROPERTY:duet_gui_fonts,INTERFACE_INCLUDE_DIRECTORIES>")

add_library(duet_gui_components STATIC
    components_src/AcceleratedSurface.cpp
    components_src/ArrangementCanvas.cpp
    components_src/BrowserCanvas.cpp
    components_src/CollaboratorPainting.cpp
    components_src/CollaboratorPanelCanvas.cpp
    components_src/GraphiteLookAndFeel.cpp
    components_src/InterTypeface.cpp
    components_src/ExportDialog.cpp
    components_src/MainShell.cpp
    components_src/MixerCanvas.cpp
    components_src/PianoRollCanvas.cpp
    components_src/PluginEditorManager.cpp
    components_src/PluginScanDialog.cpp
    components_src/SettingsWindow.cpp)
add_library(duet::gui_components ALIAS duet_gui_components)

target_include_directories(duet_gui_components PUBLIC components)
target_compile_features(duet_gui_components PRIVATE cxx_std_20)
target_link_libraries(duet_gui_components
    # duet_plugin_host_config rather than duet_juce_config: juce_audio_utils
    # brings juce_audio_processors, and this target has to compile it the way
    # duet::model does — duet_app links both (issue 4v2m38).
    PUBLIC duet::gui duet_plugin_host_config juce::juce_audio_utils juce::juce_gui_basics
    # The rendering escape hatch spec 535bbo settled: the software renderer is
    # what every surface is drawn on, and this is the way out for one that ever
    # stops holding.
    PUBLIC juce::juce_opengl
    PRIVATE duet_gui_fonts duet::model_engine_access)
