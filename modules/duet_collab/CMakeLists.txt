# duet_collab — the Collaborator service: the DAW half of the AI seam (ADR 0003).
#
# The module links no engine, no JUCE and nothing graphical. That is not thrift:
# it is what makes the real-time rule structural rather than a promise, since a
# module that cannot name an audio object cannot share a lock with the audio
# callback. It is also what lets duet_tests drive the real service over a real
# socket, which is the seam the spec names as this area's primary one.
add_library(duet_collab STATIC
    src/CollaboratorService.cpp
    src/Estimate.cpp
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

# duet_collab_analysis — the analysis routines, as pure functions.
#
# The tier-2 and tier-3 DSP of spec js437t: peak, loudness, spectrum, stereo,
# onsets, pitch, and the harmony a pitch-class profile is read for, each a pure
# function of a waveform. It links nothing at all — not the
# engine, not JUCE, not even the JSON the seam speaks — because a routine that
# can name none of those cannot share a lock with the audio callback and cannot
# be measured against anything but the signal it was handed. That is also what
# makes the spec's second test seam a plain function call.
add_library(duet_collab_analysis STATIC
    src/Analysis.cpp
    src/Harmony.cpp
    src/Loudness.cpp
    src/Onsets.cpp
    src/Pitch.cpp
    src/Spectrum.cpp)
add_library(duet::collab_analysis ALIAS duet_collab_analysis)

target_include_directories(duet_collab_analysis PUBLIC include)
target_compile_features(duet_collab_analysis PRIVATE cxx_std_20)

# duet_collab_transcription — the analysis layer's one ML dependency, and the
# one part of it the spec says may be dropped whole (spec js437t, issue bmrxnw).
#
# Its own target because of that, and not because of the audio thread: what
# changes here is what the build reaches for, so the dependency is bounded by a
# target rather than by a promise. With the option off, the header is the same
# header, the target still builds and still links, and every routine in it
# answers that it has nothing to say — which is what lets the aspects it answers
# report themselves unavailable rather than disappear.
option(DUET_ENABLE_POLYPHONIC_TRANSCRIPTION
       "Transcribe notes and name instruments with the Basic Pitch model (downloads ONNX Runtime)"
       ON)

add_library(duet_collab_transcription STATIC src/Instrument.cpp)
add_library(duet::collab_transcription ALIAS duet_collab_transcription)

target_include_directories(duet_collab_transcription PUBLIC include)
target_compile_features(duet_collab_transcription PRIVATE cxx_std_20)
target_link_libraries(duet_collab_transcription PUBLIC duet::collab_analysis)

if(DUET_ENABLE_POLYPHONIC_TRANSCRIPTION)
    include(FetchContent)

    # A prebuilt tarball and an IMPORTED target, never find_package: prototype
    # psmj4y verified that the config package ONNX Runtime ships is broken, so
    # the only honest way to name this library is to say where its two halves
    # are. The archive carries no CMakeLists.txt, so MakeAvailable downloads and
    # unpacks it and adds no subdirectory.
    FetchContent_Declare(duet_onnxruntime
        URL      https://github.com/microsoft/onnxruntime/releases/download/v1.29.0/onnxruntime-linux-x64-1.29.0.tgz
        URL_HASH SHA256=c3fddc4f139a045b0c4902c57410f0694f1c2fdf9b6939fbe38b1aeae7cd14ba
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    )

    # The Basic Pitch weights, as Spotify serialised them to ONNX at v0.4.0.
    # 230 kB, fetched rather than vendored, so that the one artifact Duet ships
    # under someone else's licence stays separately packaged — which is what the
    # licence research (lxt41c) asks for. THIRD_PARTY_NOTICES.md carries the
    # notice that goes with it.
    FetchContent_Declare(duet_basic_pitch_model
        URL      https://raw.githubusercontent.com/spotify/basic-pitch/v0.4.0/basic_pitch/saved_models/icassp_2022/nmp.onnx
        URL_HASH SHA256=2c3c1d144bfa61ad236e92e169c13535c880469a12a047d4e73451f2c059a0ec
        DOWNLOAD_NO_EXTRACT TRUE
    )

    FetchContent_MakeAvailable(duet_onnxruntime duet_basic_pitch_model)

    set(DUET_ONNXRUNTIME_LIBRARY "${duet_onnxruntime_SOURCE_DIR}/lib/libonnxruntime.so.1"
        CACHE INTERNAL "The ONNX Runtime shared library the DAW loads")
    set(DUET_BASIC_PITCH_MODEL "${duet_basic_pitch_model_SOURCE_DIR}/nmp.onnx"
        CACHE INTERNAL "The Basic Pitch weights the transcription reads")

    add_library(duet_onnxruntime_imported SHARED IMPORTED GLOBAL)
    set_target_properties(duet_onnxruntime_imported PROPERTIES
        IMPORTED_LOCATION "${DUET_ONNXRUNTIME_LIBRARY}"
        IMPORTED_SONAME "libonnxruntime.so.1"
        INTERFACE_INCLUDE_DIRECTORIES "${duet_onnxruntime_SOURCE_DIR}/include")
    add_library(duet::onnxruntime ALIAS duet_onnxruntime_imported)

    target_sources(duet_collab_transcription PRIVATE src/BasicPitch.cpp)
    target_link_libraries(duet_collab_transcription PRIVATE duet::onnxruntime)

    # Where the weights are in this build tree. The routine looks beside the
    # running executable first, which is where the install puts them, and falls
    # back to this — so the tests and a Duet run out of the build tree both find
    # them without anything being copied anywhere first.
    target_compile_definitions(duet_collab_transcription PRIVATE
        DUET_BASIC_PITCH_MODEL="${DUET_BASIC_PITCH_MODEL}")
else()
    message(STATUS "Duet: polyphonic transcription is off "
                   "(DUET_ENABLE_POLYPHONIC_TRANSCRIPTION=OFF)")
    target_sources(duet_collab_transcription PRIVATE src/NoBasicPitch.cpp)
endif()

# duet_collab_tools — the Tool Vocabulary answered from the live project model.
#
# A second target rather than more of the first, and the split is the same one
# duet_gui makes: the service above must link no engine, because a module that
# cannot name an audio object cannot share a lock with the audio callback, and
# reading the project means linking the model, which links the engine. So the
# reads live here and the socket lives there, and the only thing crossing
# between them is JSON and a function that puts a read on the message thread.
add_library(duet_collab_tools STATIC
    src/AnalysisCall.cpp
    src/ContentEstimates.cpp
    src/ProjectTools.cpp
    src/SuggestTool.cpp
    src/SuggestionManager.cpp
    src/TrackAnalysis.cpp
    src/TrackRenders.cpp)
add_library(duet::collab_tools ALIAS duet_collab_tools)

target_compile_features(duet_collab_tools PRIVATE cxx_std_20)
target_link_libraries(duet_collab_tools PUBLIC
    duet::collab duet::collab_analysis duet::collab_transcription duet::model)
