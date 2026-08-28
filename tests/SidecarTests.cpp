// The real sidecar, driven by the real service over a real socket.
//
// Everything else in the Collaborator suites points the DAW at
// `tests/sidecar_double`, which is what proves that anything speaking the
// protocol can take the sidecar's place. This suite is the other half of that
// claim: the shipped binary is one of those things.
//
// No provider is reached. The sidecar's `--offline-script` puts a scripted model
// where the provider would be — the mirror of the double, from the far end — so
// a run, its tool calls, its commentary and its cancellation are all driven from
// where the model would be driving them, and nothing here costs a token or
// touches the network. What is left over is one live-provider run, which is the
// issue's own closing criterion and is made by hand.
//
// The whole suite skips itself when the binary was not built, which is what a
// machine without bun sees.

#include "CollaboratorHarness.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

using duet::collab::Json;
using duet::collab::OpeningContext;
using duet::collab::RunStatus;
using duet::collab::SelectionKind;
using duet::collab::ToolPhase;
using duet::testing::Harness;
using duet::testing::RecordingListener;
using duet::testing::waitUntil;
using namespace std::chrono_literals;

namespace
{
#ifdef DUET_SIDECAR_BINARY
constexpr std::string_view sidecarBinary = DUET_SIDECAR_BINARY;
#else
constexpr std::string_view sidecarBinary;
#endif

/** Whether there is a sidecar to drive. */
bool sidecarWasBuilt()
{
    return ! sidecarBinary.empty() && std::filesystem::exists (sidecarBinary);
}

/** A folder to keep a test's scripts and dumps in, removed with this object. */
class TempFiles
{
public:
    TempFiles()
    {
        static int counter = 0;

        folder =
            std::filesystem::temp_directory_path()
            / ("duet-sidecar-" + std::to_string (::getpid()) + "-" + std::to_string (++counter));

        std::filesystem::create_directories (folder);
    }

    ~TempFiles()
    {
        std::error_code ignored;
        std::filesystem::remove_all (folder, ignored);
    }

    TempFiles (const TempFiles&) = delete;
    TempFiles& operator= (const TempFiles&) = delete;

    /** Writes a file and answers where it went. */
    [[nodiscard]] std::filesystem::path write (const std::string& name,
                                               const std::string& contents) const
    {
        const auto path = folder / name;
        std::ofstream file { path };
        file << contents;

        return path;
    }

    [[nodiscard]] std::filesystem::path at (const std::string& name) const { return folder / name; }

private:
    std::filesystem::path folder;
};

/** A service pointed at the real sidecar, running a scripted model. */
Harness
    scripted (const TempFiles& files, const Json& script, const std::string& name = "script.json")
{
    const auto path = files.write (name, script.dump());

    return Harness { "--offline-script", { path.string() }, sidecarBinary };
}

/** Runs the binary with those arguments and answers everything it printed. */
std::string capture (const std::string& arguments)
{
    const auto command = std::string { sidecarBinary } + " " + arguments + " 2>&1";

    // NOLINTNEXTLINE(cert-env33-c) — the command is this build's own binary path.
    auto* pipe = ::popen (command.c_str(), "r");

    if (pipe == nullptr)
        return {};

    std::string output;
    std::array<char, 4096> chunk {};

    while (std::fgets (chunk.data(), static_cast<int> (chunk.size()), pipe) != nullptr)
        output += chunk.data();

    ::pclose (pipe);

    return output;
}

/** The assembled prompt and tool list the sidecar would give a model. */
Json dumpPrompt (const std::string& parameters = "{}")
{
    return Json::parse (capture ("--dump-prompt --params '" + parameters + "'"), nullptr, false);
}

std::string readWhole (const std::filesystem::path& path)
{
    const std::ifstream file { path };
    std::ostringstream contents;
    contents << file.rdbuf();

    return contents.str();
}

/** A producer who had one clip selected, at bar 9, with the transport stopped. */
OpeningContext atBarNine()
{
    OpeningContext context;
    context.selection = SelectionKind::clips;
    context.selectionIds = { "clip-3" };
    context.playheadBar = 9;
    context.playheadBeat = 1.0;

    return context;
}

/** One turn of a scripted model: it says something, and stops. */
Json says (const std::string& text) { return Json { { "text", text } }; }

/** One turn of a scripted model: it says something and calls a tool, so the
    loop answers the tool and comes back for the next turn.
*/
Json saysAndCalls (const std::string& text,
                   const std::string& tool,
                   const Json& arguments = Json::object())
{
    return Json { { "text", text },
                  { "toolCall", Json { { "name", tool }, { "arguments", arguments } } } };
}
} // namespace

//==============================================================================
TEST_CASE ("the DAW spawns the shipped sidecar and configuration round-trips", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    auto harness = scripted (files, Json::array ({ says ("ready") }));

    harness->start();

    const auto configured =
        harness->configure ("duet-offline:scripted", Json { { "project", "Fixture A" } });

    CHECK (configured.succeeded);
    CHECK (harness->isSidecarRunning());
    CHECK (harness->sidecarProcessId().has_value());
}

TEST_CASE ("a run's commentary streams from the sidecar to the DAW-side listener", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    RecordingListener listener;
    auto harness = scripted (files, Json::array ({ says ("Something is off in the drop.") }));

    harness->setTaskRunListener (&listener);
    harness->start();
    REQUIRE (harness->configure ("duet-offline:scripted", Json::object()).succeeded);

    const auto started = harness->startRun ("what's off in the drop?", atBarNine());

    REQUIRE (started.started);
    REQUIRE (listener.waitForTerminals (1, 15s));

    CHECK (listener.commentary() == "Something is off in the drop.");

    const auto terminals = listener.of (RecordingListener::Kind::terminal);
    REQUIRE (terminals.size() == 1);
    CHECK (terminals.front().status == RunStatus::completed);
    CHECK (terminals.front().runId == started.runId);

    harness->setTaskRunListener (nullptr);
}

TEST_CASE ("a tool the model calls crosses to the DAW and its result reaches the model",
           "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    const auto contextDump = files.at ("context.json");
    RecordingListener listener;

    const auto script = files.write ("script.json",
                                     Json::array ({ saysAndCalls ("Let me look.", "list_tracks"),
                                                    says ("The kick is doing all the work.") })
                                         .dump());

    const Harness harness { "--offline-script",
                            { script.string(), "--dump-context", contextDump.string() },
                            sidecarBinary };

    // The one answer the DAW gives: a track list with a name in it that could
    // have come from nowhere else, so finding it in the model's transcript is
    // proof that the result went back rather than that something plausible did.
    harness->setMethodHandler (
        "tool.call",
        [] (const Json& params)
        {
            CHECK (params.value ("tool", std::string {}) == "list_tracks");

            return duet::collab::RpcOutcome::success (Json {
                { "tracks",
                  Json::array ({ Json { { "id", "track-1" }, { "name", "Ganymede Kick" } } }) } });
        });

    harness->setTaskRunListener (&listener);
    harness->start();
    REQUIRE (harness->configure ("duet-offline:scripted", Json::object()).succeeded);
    REQUIRE (harness->startRun ("what have I got?", {}).started);
    REQUIRE (listener.waitForTerminals (1, 15s));

    const auto activity = listener.of (RecordingListener::Kind::tool);
    REQUIRE (activity.size() == 2);
    CHECK (activity.at (0).text == "list_tracks");
    CHECK (activity.at (0).phase == ToolPhase::start);
    CHECK (activity.at (1).phase == ToolPhase::end);

    CHECK (listener.commentary() == "Let me look.The kick is doing all the work.");

    REQUIRE (waitUntil ([&contextDump] { return std::filesystem::exists (contextDump); }));

    CHECK_THAT (readWhole (contextDump), Catch::Matchers::ContainsSubstring ("Ganymede Kick"));

    harness->setTaskRunListener (nullptr);
}

TEST_CASE ("the model's tools are the Tool Vocabulary and the write-tool and nothing else",
           "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const auto dump = dumpPrompt();

    REQUIRE (dump.contains ("tools"));

    std::vector<std::string> named;

    for (const auto& tool : dump.at ("tools"))
        named.push_back (tool.value ("name", std::string {}));

    const std::vector<std::string> expected { "list_tracks",
                                              "get_arrangement",
                                              "get_midi",
                                              "get_plugin_chain",
                                              "get_automation",
                                              "get_track_analysis",
                                              "estimate_audio_content",
                                              "suggest" };

    CHECK (named == expected);
}

TEST_CASE ("the system prompt in force is Duet's own", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const auto prompt = dumpPrompt().value ("systemPrompt", std::string {});

    SECTION ("it says who the Collaborator is")
    {
        CHECK_THAT (prompt, Catch::Matchers::ContainsSubstring ("You are the Collaborator"));
        CHECK_THAT (prompt, Catch::Matchers::ContainsSubstring ("You never hear audio"));
    }

    SECTION ("it carries the provenance rules and the instruction to hedge")
    {
        CHECK_THAT (prompt, Catch::Matchers::ContainsSubstring ("\"source\": \"estimated\""));
        CHECK_THAT (prompt, Catch::Matchers::ContainsSubstring ("A bare value"));
        CHECK_THAT (prompt, Catch::Matchers::ContainsSubstring ("is a fact"));
        CHECK_THAT (prompt, Catch::Matchers::ContainsSubstring ("Hedge when you should"));
    }

    SECTION ("nothing in it belongs to a coding agent")
    {
        // The first is pi-ai's own default, which its provider adapters put in
        // place of an empty prompt (measured at d8k46e); the rest are the words
        // an agent for programmers would be built out of.
        for (const auto* absent : { "helpful assistant",
                                    "coding agent",
                                    "software engineering",
                                    "codebase",
                                    "repository",
                                    "source code",
                                    "the user's" })
            CHECK_THAT (prompt, ! Catch::Matchers::ContainsSubstring (absent));
    }
}

TEST_CASE ("the prompt the provider receives is the prompt the sidecar assembled", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    const auto contextDump = files.at ("context.json");
    RecordingListener listener;

    const auto script = files.write ("script.json", Json::array ({ says ("looked") }).dump());

    const Harness harness { "--offline-script",
                            { script.string(), "--dump-context", contextDump.string() },
                            sidecarBinary };

    harness->setTaskRunListener (&listener);
    harness->start();
    REQUIRE (harness->configure ("duet-offline:scripted", Json { { "project", "Fixture A" } })
                 .succeeded);
    REQUIRE (harness->startRun ("hello", {}).started);
    REQUIRE (listener.waitForTerminals (1, 15s));
    REQUIRE (waitUntil ([&contextDump] { return std::filesystem::exists (contextDump); }));

    const auto seen = Json::parse (readWhole (contextDump), nullptr, false);

    REQUIRE (seen.is_object());

    // What the far side of the agent was handed, rather than what this side
    // meant to hand it: the same prompt, and the same eight tools.
    CHECK (seen.value ("systemPrompt", std::string {})
           == dumpPrompt (R"({"project":"Fixture A"})").value ("systemPrompt", std::string {}));
    CHECK (seen.at ("tools").size() == 8);

    harness->setTaskRunListener (nullptr);
}

TEST_CASE ("the prompt keeps a frozen prefix and puts what moves at the end", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const auto once = dumpPrompt (R"({"project":"Fixture A"})");
    const auto again = dumpPrompt (R"({"project":"Fixture A"})");
    const auto elsewhere = dumpPrompt (R"({"project":"Fixture B"})");
    const auto bare = dumpPrompt();

    const auto first = once.value ("systemPrompt", std::string {});
    const auto second = again.value ("systemPrompt", std::string {});
    const auto other = elsewhere.value ("systemPrompt", std::string {});
    const auto frozen = bare.value ("systemPrompt", std::string {});

    SECTION ("the same project state produces the same bytes")
    {
        CHECK (first == second);
        CHECK (once.dump() == again.dump());
    }

    SECTION ("what moves moves only after everything that does not")
    {
        CHECK (first != other);
        CHECK (first.rfind (frozen, 0) == 0);
        CHECK (other.rfind (frozen, 0) == 0);
        CHECK_THAT (first.substr (frozen.size()), Catch::Matchers::ContainsSubstring ("Fixture A"));
        CHECK_THAT (frozen, ! Catch::Matchers::ContainsSubstring ("Fixture A"));
    }
}

TEST_CASE ("cancelling a run aborts the provider request and nothing follows the ending",
           "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    RecordingListener listener;

    // Slow enough that the cancel lands with the answer half-written, which is
    // the only arrangement under which "the request in flight was aborted" is a
    // claim about anything.
    auto harness = scripted (
        files,
        Json {
            { "tokensPerSecond", 6 },
            { "steps",
              Json::array ({ says ("one two three four five six seven eight nine ten eleven twelve "
                                   "thirteen fourteen fifteen sixteen"),
                             says ("never reached") }) } });

    harness->setTaskRunListener (&listener);
    harness->start();
    REQUIRE (harness->configure ("duet-offline:scripted", Json::object()).succeeded);

    const auto started = harness->startRun ("say a lot", {});
    REQUIRE (started.started);

    REQUIRE (waitUntil (
        [&listener] { return ! listener.of (RecordingListener::Kind::commentary).empty(); }, 15s));

    CHECK (harness->cancelRun (started.runId));
    REQUIRE (listener.waitForTerminals (1));

    const auto whole = listener.events();
    std::size_t terminals = 0;
    std::size_t afterTheEnding = 0;

    for (const auto& event : whole)
    {
        if (event.kind == RecordingListener::Kind::terminal)
            ++terminals;
        else if (terminals > 0)
            ++afterTheEnding;
    }

    CHECK (terminals == 1);
    CHECK (afterTheEnding == 0);
    CHECK (whole.back().status == RunStatus::canceled);
    CHECK_THAT (listener.commentary(), ! Catch::Matchers::ContainsSubstring ("never reached"));

    // The provider request stopped rather than merely being ignored: the whole
    // answer would have taken far longer than the moment we waited here.
    std::this_thread::sleep_for (500ms);
    CHECK (listener.of (RecordingListener::Kind::terminal).size() == 1);

    harness->setTaskRunListener (nullptr);
}

TEST_CASE ("a provider that refuses fails the run and leaves the sidecar usable", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    RecordingListener listener;

    auto harness =
        scripted (files,
                  Json::array ({ Json { { "error", "the provider refused the request" } },
                                 says ("and now it works") }));

    harness->setTaskRunListener (&listener);
    harness->start();
    REQUIRE (harness->configure ("duet-offline:scripted", Json::object()).succeeded);
    REQUIRE (harness->startRun ("first", {}).started);
    REQUIRE (listener.waitForTerminals (1, 15s));

    const auto refused = listener.of (RecordingListener::Kind::terminal).front();
    CHECK (refused.status == RunStatus::failed);
    CHECK (refused.error == "the provider refused the request");

    const auto stillThere = harness->sidecarProcessId();
    REQUIRE (stillThere.has_value());

    REQUIRE (harness->startRun ("second", {}).started);
    REQUIRE (listener.waitForTerminals (2, 15s));

    const auto endings = listener.of (RecordingListener::Kind::terminal);
    REQUIRE (endings.size() == 2);
    CHECK (endings.at (1).status == RunStatus::completed);
    CHECK_THAT (listener.commentary(), Catch::Matchers::ContainsSubstring ("and now it works"));
    CHECK (harness->sidecarProcessId() == stillThere);

    harness->setTaskRunListener (nullptr);
}

TEST_CASE ("the sidecar says nothing on the DAW's own streams", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    RecordingListener listener;

    const auto script = files.write (
        "script.json",
        Json::array ({ saysAndCalls ("looking", "get_arrangement"), says ("done") }).dump());

    const auto out = files.at ("out.txt");
    const auto err = files.at ("err.txt");

    // The service spawns a child that inherits its streams, so the only way to
    // see what that child would have written on them is to put a redirection in
    // between. The wrapper is what the service launches; the sidecar is what
    // runs, with the socket path it was given.
    const auto wrapper =
        files.write ("wrapper.sh",
                     std::string { "#!/bin/sh\nexec '" } + std::string { sidecarBinary }
                         + "' \"$@\" > '" + out.string() + "' 2> '" + err.string() + "'\n");

    std::filesystem::permissions (wrapper, std::filesystem::perms::owner_all);

    const Harness harness { "--offline-script", { script.string() }, wrapper };

    harness->setMethodHandler (
        "tool.call",
        [] (const Json&)
        { return duet::collab::RpcOutcome::success (Json { { "tempoBpm", 128 } }); });

    harness->setTaskRunListener (&listener);
    harness->start();
    REQUIRE (harness->configure ("duet-offline:scripted", Json::object()).succeeded);
    REQUIRE (harness->startRun ("anything", atBarNine()).started);
    REQUIRE (listener.waitForTerminals (1, 15s));

    CHECK (readWhole (out).empty());
    CHECK (readWhole (err).empty());

    harness->setTaskRunListener (nullptr);
}

TEST_CASE ("a sidecar killed mid-run fails that run and the next one replaces it", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    const TempFiles files;
    RecordingListener listener;

    auto harness =
        scripted (files,
                  Json { { "tokensPerSecond", 4 },
                         { "steps",
                           Json::array ({ says ("one two three four five six seven eight nine ten"),
                                          says ("a second run") }) } });

    harness->setTaskRunListener (&listener);
    harness->start();
    REQUIRE (harness->configure ("duet-offline:scripted", Json::object()).succeeded);
    REQUIRE (harness->startRun ("say a lot", {}).started);

    const auto killed = harness->sidecarProcessId();
    REQUIRE (killed.has_value());

    const auto killedId = killed.value_or (0);

    REQUIRE (waitUntil (
        [&listener] { return ! listener.of (RecordingListener::Kind::commentary).empty(); }, 15s));

    ::kill (static_cast<pid_t> (killedId), SIGKILL);

    REQUIRE (listener.waitForTerminals (1, 15s));
    CHECK (listener.of (RecordingListener::Kind::terminal).front().status == RunStatus::failed);

    // The DAW is untouched by it: the next run is asked for in the ordinary way
    // and spawns the replacement.
    REQUIRE (harness->configure ("duet-offline:scripted", Json::object()).succeeded);
    CHECK (harness->sidecarProcessId() != killed);

    REQUIRE (harness->startRun ("again", {}).started);
    REQUIRE (listener.waitForTerminals (2, 15s));
    CHECK (listener.of (RecordingListener::Kind::terminal).at (1).status == RunStatus::completed);

    harness->setTaskRunListener (nullptr);
}

TEST_CASE ("the sidecar needs nothing of the machine it runs on", "[sidecar]")
{
    if (! sidecarWasBuilt())
        SKIP ("the sidecar was not built — no bun on this machine");

    // The prototype proved the strong form of this in a container with no node
    // installed (d8k46e). What is left to keep true here is that nothing crept
    // in since: an empty environment and an empty PATH are enough for it.
    const auto command = "env -i PATH= '" + std::string { sidecarBinary } + "' --dump-prompt 2>&1";

    // NOLINTNEXTLINE(cert-env33-c) — the command is this build's own binary path.
    auto* pipe = ::popen (command.c_str(), "r");
    REQUIRE (pipe != nullptr);

    std::string output;
    std::array<char, 4096> chunk {};

    while (std::fgets (chunk.data(), static_cast<int> (chunk.size()), pipe) != nullptr)
        output += chunk.data();

    REQUIRE (::pclose (pipe) == 0);

    CHECK_THAT (output, Catch::Matchers::ContainsSubstring ("You are the Collaborator"));
}
