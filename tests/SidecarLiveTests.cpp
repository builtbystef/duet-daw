// One Task Run, all the way through: a real project, the real tools, the real
// sidecar, and a real model provider.
//
// The one case here is hidden — Catch2 runs a `[.live]` case only when it is
// asked for by name or tag — because it costs money, needs credentials and
// reaches the network, and none of those belong in a suite the push gate runs.
// Everything it exercises apart from the provider is asserted without it in
// `SidecarTests.cpp`; what this adds is the half no double can stand in for,
// which is a model that actually decides what to ask.
//
// Run it, with a model whichever provider your environment configures:
//
//     DUET_LIVE_MODEL=openai:gpt-5.6 ./build/tests/Debug/duet_tests "[live]"
//
// It prints the commentary and the tool trace, which is what issue oocnng's
// closing criterion asks to be recorded as a note.
//
// DUET_LIVE_SCRIPT names an offline script instead, which puts a scripted model
// where the provider goes. That is not the criterion — it is how this harness
// itself is kept honest without spending anything.

#include "ProjectToolsHarness.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

using duet::collab::Json;
using duet::collab::OpeningContext;
using duet::collab::ProjectTools;
using duet::collab::RunStatus;
using duet::collab::SelectionKind;
using duet::collab::SuggestTool;
using duet::collab::ToolRegistry;
using duet::model::BuiltinPlugin;
using duet::model::Session;
using duet::model::TrackKind;
using duet::testing::Harness;
using duet::testing::messageThreadMarshal;
using duet::testing::pumpUntil;
using duet::testing::RunEnding;
using duet::testing::TempProject;

namespace
{
#ifdef DUET_SIDECAR_BINARY
constexpr std::string_view liveSidecar = DUET_SIDECAR_BINARY;
#else
constexpr std::string_view liveSidecar;
#endif

/** How long a live run is given. A provider takes seconds per turn and a run
    that reads a project takes several turns.
*/
constexpr int liveRunTimeoutMs = 180000;

/** An environment variable, or nothing.

    Read before the case starts anything, which is the same single-threaded
    moment TestMain reads its own.
*/
std::string fromEnvironment (const char* name)
{
    const auto* const value = std::getenv (name); // NOLINT(concurrency-mt-unsafe)

    return value == nullptr ? std::string {} : std::string { value };
}

/** A small project with a recognisable problem in it: a kick and a bass that
    are both loud and both low, under a pad that has been pushed out of the way.

    Built rather than loaded, because what this case needs of a project is only
    that it is real and that there is something in it worth saying.
*/
void buildTheProject (Session& session)
{
    session.performAction (
        "build the live fixture",
        [] (duet::model::EditOps& ops)
        {
            ops.setTempo (128.0);

            const auto kick = ops.createTrack (TrackKind::midi, "Kick", BuiltinPlugin::sampler);
            const auto bass = ops.createTrack (TrackKind::midi, "Bass", BuiltinPlugin::synth);
            const auto pad = ops.createTrack (TrackKind::midi, "Pad", BuiltinPlugin::synth);

            ops.setTrackVolumeDb (kick, -3.0);
            ops.setTrackVolumeDb (bass, -3.5);
            ops.setTrackVolumeDb (pad, -14.0);
            ops.setTrackPan (pad, -0.3);

            const auto kickClip = ops.insertMidiClip (kick, "Kick 4-on-the-floor", 0.0, 7.5);

            for (int beat = 0; beat < 16; ++beat)
                ops.addNote (kickClip, 36, static_cast<double> (beat), 0.25, 110);

            const auto bassClip = ops.insertMidiClip (bass, "Bass", 0.0, 7.5);

            for (int beat = 0; beat < 16; ++beat)
                ops.addNote (
                    bassClip, beat % 4 == 0 ? 41 : 36, static_cast<double> (beat), 0.9, 100);

            const auto padClip = ops.insertMidiClip (pad, "Pad", 0.0, 7.5);

            for (const auto pitch : { 65, 69, 72 })
                ops.addNote (padClip, pitch, 0.0, 16.0, 70);

            ops.addPlugin (bass, BuiltinPlugin::compressor, 0);
        });
}
} // namespace

TEST_CASE ("the Collaborator answers about a real project through a real provider", "[.live]")
{
    if (liveSidecar.empty() || ! std::filesystem::exists (liveSidecar))
        SKIP ("the sidecar was not built — no bun on this machine");

    const auto model = fromEnvironment ("DUET_LIVE_MODEL");
    const auto script = fromEnvironment ("DUET_LIVE_SCRIPT");

    if (model.empty())
        SKIP (
            "set DUET_LIVE_MODEL=provider:id, with that provider's credentials in the environment");

    const TempProject project;
    Session session { project.editFile() };
    buildTheProject (session);

    ToolRegistry registry;
    ProjectTools reads { session, messageThreadMarshal() };
    SuggestTool writes { session, messageThreadMarshal() };
    reads.addTo (registry);
    writes.addTo (registry);

    RunEnding ending;

    std::vector<std::string> arguments;

    if (! script.empty())
        arguments = { "--offline-script", script };

    const Harness harness { arguments, std::filesystem::path { liveSidecar } };

    // The whole trace, kept as it happens: what the model asked for, and what it
    // was told. It is the evidence that the commentary is grounded rather than
    // invented, so it is printed whether the case passes or not.
    std::vector<std::string> trace;

    harness->setMethodHandler (
        "tool.call",
        [&registry, &trace] (const Json& params)
        {
            auto answer = registry.call (params);
            const auto called = params.value ("tool", std::string {});
            const auto given = params.value ("args", Json::object()).dump();
            const auto said =
                answer.succeeded ? answer.result.dump() : "REFUSED " + answer.error.message;

            trace.push_back (called + " " + given + "\n    -> " + said.substr (0, 600));

            return answer;
        });

    harness->setTaskRunListener (&ending);
    harness->start();

    const auto configured = harness->configure (model, Json { { "project", "Live fixture" } });
    INFO ("configure: " << configured.error.message);
    REQUIRE (configured.succeeded);

    OpeningContext context;
    context.selection = SelectionKind::none;
    context.playheadBar = 1;

    const auto started =
        harness->startRun ("The low end feels crowded to me. What's going on down there?", context);
    REQUIRE (started.started);

    const auto ran = pumpUntil ([&ending] { return ending.hasEnded(); }, liveRunTimeoutMs);

    std::cout << "\n=== the Collaborator said ===\n"
              << ending.commentary() << "\n\n=== the tool trace ===\n";

    for (const auto& entry : trace)
        std::cout << "  " << entry << "\n";

    std::cout << "\n=== the run ended ===\n"
              << (ending.status() == RunStatus::completed ? "completed" : ending.error()) << "\n\n";

    harness->setTaskRunListener (nullptr);

    REQUIRE (ran);
    INFO ("the run ended with: " << ending.error());
    CHECK (ending.status() == RunStatus::completed);

    // Grounded: it looked before it spoke, and it spoke.
    CHECK_FALSE (trace.empty());
    CHECK_FALSE (ending.commentary().empty());
}
