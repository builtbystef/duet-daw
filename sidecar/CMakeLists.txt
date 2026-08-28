# The sidecar: pi's agent loop as one standalone binary, built by bun and shipped
# inside the DAW install (ADR 0003, prototype d8k46e).
#
# It is a JavaScript program, so it is built by its own toolchain rather than by
# the compiler, and it is optional in exactly one direction: a machine without
# bun still builds and tests the whole DAW, and the suite's sidecar tests skip
# themselves rather than fail. What is not optional is the protocol — the service
# is verified against tests/sidecar_double whether this binary exists or not.
#
# `bun install` reaches the network, the way FetchContent does for JUCE and
# Tracktion, and bun.lock is what pins what it fetches.

option(DUET_SIDECAR_ENABLED "Build the pi sidecar binary (needs bun on the PATH)" ON)

if(NOT DUET_SIDECAR_ENABLED)
    message(STATUS "Duet: the sidecar is off (DUET_SIDECAR_ENABLED=OFF)")
    return()
endif()

find_program(DUET_BUN bun)

if(NOT DUET_BUN)
    message(STATUS "Duet: no bun on the PATH, so the sidecar is not built — its tests will skip")
    return()
endif()

# Which runtime bun compiles in. `baseline` is the x86-64 build without the
# newer instruction sets, which is what a binary shipped to other people's
# machines wants; every other host takes bun's own default for itself, because
# cross-platform distribution is not milestone-one work.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    set(DUET_SIDECAR_BUN_TARGET "bun-linux-x64-baseline" CACHE STRING "The runtime bun compiles into the sidecar")
else()
    set(DUET_SIDECAR_BUN_TARGET "" CACHE STRING "The runtime bun compiles into the sidecar")
endif()

if(DUET_SIDECAR_BUN_TARGET)
    set(duet_sidecar_target_flag "--target=${DUET_SIDECAR_BUN_TARGET}")
else()
    set(duet_sidecar_target_flag "")
endif()

set(DUET_SIDECAR_BINARY "${CMAKE_CURRENT_BINARY_DIR}/duet-sidecar" CACHE INTERNAL "The built sidecar")

file(GLOB duet_sidecar_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.ts")

add_custom_command(
    OUTPUT "${DUET_SIDECAR_BINARY}"
    COMMAND "${DUET_BUN}" install --frozen-lockfile --silent
    # No --sourcemap flag: bun 1.3.10 reads --sourcemap=none as a request for an
    # external map and writes a 10 MB one beside the binary. Leaving it out is
    # what actually means none.
    COMMAND "${DUET_BUN}" build --compile ${duet_sidecar_target_flag} --minify
            "${CMAKE_CURRENT_SOURCE_DIR}/src/main.ts" --outfile "${DUET_SIDECAR_BINARY}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS ${duet_sidecar_sources}
            "${CMAKE_CURRENT_SOURCE_DIR}/package.json"
            "${CMAKE_CURRENT_SOURCE_DIR}/bun.lock"
    COMMENT "Building the Duet sidecar with bun"
    VERBATIM)

add_custom_target(duet_sidecar ALL DEPENDS "${DUET_SIDECAR_BINARY}")
