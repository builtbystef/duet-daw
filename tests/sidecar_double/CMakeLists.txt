# The test-double sidecar: a program that speaks the socket protocol and nothing
# else. It is built for the suite to spawn, not run by hand, and it links only a
# JSON library — deliberately no part of the service it is pointed at, so that
# an agreement between the two is evidence about the protocol.
add_executable(duet_sidecar_double main.cpp)

target_compile_features(duet_sidecar_double PRIVATE cxx_std_20)
target_link_libraries(duet_sidecar_double PRIVATE nlohmann_json::nlohmann_json)
