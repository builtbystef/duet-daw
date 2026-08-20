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
    src/SidecarProcess.cpp)
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
