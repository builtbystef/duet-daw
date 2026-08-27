# duet_collab — the Collaborator service: the DAW half of the AI seam (ADR 0003).
#
# The module links no engine, no JUCE and nothing graphical. That is not thrift:
# it is what makes the real-time rule structural rather than a promise, since a
# module that cannot name an audio object cannot share a lock with the audio
# callback. It is also what lets duet_tests drive the real service over a real
# socket, which is the seam the spec names as this area's primary one.
add_library(duet_collab STATIC
    src/CollaboratorService.cpp
    src/LineReader.cpp
    src/LocalSocketServer.cpp
    src/SidecarProcess.cpp
    src/ToolDispatch.cpp)
add_library(duet::collab ALIAS duet_collab)

target_include_directories(duet_collab PUBLIC include)
target_compile_features(duet_collab PRIVATE cxx_std_20)

# JSON is the payload vocabulary of the seam, so it reaches the public header and
# the link is PUBLIC. The include directory is marked SYSTEM: a vendored header
# is not Duet's to lint, and without this the sweep would report the library's
# own code as ours.
target_link_libraries(duet_collab PUBLIC nlohmann_json::nlohmann_json)

find_package(Threads REQUIRED)
target_link_libraries(duet_collab PRIVATE Threads::Threads)

# duet_collab_tools — the Tool Vocabulary answered from the live project model.
#
# A second target rather than more of the first, and the split is the same one
# duet_gui makes: the service above must link no engine, because a module that
# cannot name an audio object cannot share a lock with the audio callback, and
# reading the project means linking the model, which links the engine. So the
# reads live here and the socket lives there, and the only thing crossing
# between them is JSON and a function that puts a read on the message thread.
add_library(duet_collab_tools STATIC
    src/ProjectTools.cpp
    src/SuggestTool.cpp)
add_library(duet::collab_tools ALIAS duet_collab_tools)

target_compile_features(duet_collab_tools PRIVATE cxx_std_20)
target_link_libraries(duet_collab_tools PUBLIC duet::collab duet::model)
