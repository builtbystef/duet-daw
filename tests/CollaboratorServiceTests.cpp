#include "CollaboratorHarness.h"

#include <catch2/catch_test_macros.hpp>

#include <poll.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <thread>

using duet::collab::Json;
using duet::testing::connectDirectly;
using duet::testing::Harness;
using duet::testing::processExists;
using duet::testing::waitUntil;
using namespace std::chrono_literals;

TEST_CASE ("a sidecar connects to the started service and answers configure", "[collab]")
{
    const Harness harness;
    harness->start();

    const auto outcome =
        harness->configure ("gpt-5.6-terra", Json { { "projectName", "Untitled" } });

    REQUIRE (outcome.succeeded);
    REQUIRE (outcome.result.is_object());
}

TEST_CASE ("two messages in one write are two requests", "[collab]")
{
    Harness harness { "two-in-one-write" };
    harness->start();

    REQUIRE (harness->configure ("gpt-5.6-terra", Json::object()).succeeded);
    REQUIRE (harness.waitForReports (2));

    const auto reports = harness.reports();

    REQUIRE (reports.size() == 2);
    REQUIRE (reports.at (0).at ("tag") == "first");
    REQUIRE (reports.at (1).at ("tag") == "second");
}

TEST_CASE ("a newline inside a string value does not end the message", "[collab]")
{
    Harness harness { "escaped-newline" };
    harness->start();

    REQUIRE (harness->configure ("gpt-5.6-terra", Json::object()).succeeded);
    REQUIRE (harness.waitForReports (1));

    const auto reports = harness.reports();

    REQUIRE (reports.size() == 1);
    REQUIRE (reports.at (0).at ("payload").at ("text") == "one\ntwo");
}

TEST_CASE ("a message split across writes is handled when its newline arrives", "[collab]")
{
    Harness harness { "split-write" };
    harness->start();

    // Answering this starts the double writing a report in three pieces, 400 ms
    // apart, with the newline last.
    REQUIRE (harness->configure ("gpt-5.6-terra", Json::object()).succeeded);

    // Halfway through, the service is holding a message that is complete except
    // for its terminator, and has done nothing with it.
    std::this_thread::sleep_for (1000ms);
    REQUIRE (harness.reports().empty());

    // The newline is what makes it a message.
    REQUIRE (harness.waitForReports (1));
    REQUIRE (harness.reports().at (0).at ("tag") == "split");
}

TEST_CASE ("a malformed line is a parse error and the connection survives", "[collab]")
{
    Harness harness { "malformed-line" };
    harness->start();

    REQUIRE (harness->configure ("gpt-5.6-terra", Json::object()).succeeded);
    REQUIRE (harness.waitForReports (1));

    const auto answer = harness.reports().at (0).at ("payload");

    REQUIRE (answer.at ("error").at ("code") == -32700);
    REQUIRE (answer.at ("id").is_null());

    // The report itself arrived over the same connection, and it still answers.
    REQUIRE (harness->configure ("gpt-5.6-terra", Json::object()).succeeded);
}

TEST_CASE ("an unknown method is a method-not-found error and the connection survives", "[collab]")
{
    Harness harness { "unknown-method" };
    harness->start();

    REQUIRE (harness->configure ("gpt-5.6-terra", Json::object()).succeeded);
    REQUIRE (harness.waitForReports (1));

    const auto answer = harness.reports().at (0).at ("payload");

    REQUIRE (answer.at ("error").at ("code") == -32601);
    REQUIRE (answer.at ("id") == 906);

    REQUIRE (harness->configure ("gpt-5.6-terra", Json::object()).succeeded);
}

TEST_CASE ("a session that never asks the Collaborator anything spawns no sidecar", "[collab]")
{
    const Harness harness;
    harness->start();

    std::this_thread::sleep_for (250ms);

    REQUIRE (std::filesystem::exists (harness.socketPath()));
    REQUIRE_FALSE (harness->isSidecarRunning());
    REQUIRE_FALSE (harness->sidecarProcessId().has_value());
}

TEST_CASE ("a sidecar killed from outside is replaced by the next use", "[collab]")
{
    Harness harness;
    harness->start();

    REQUIRE (harness->configure ("gpt-5.6-terra", Json::object()).succeeded);

    const auto first = harness->sidecarProcessId().value_or (0);
    REQUIRE (first != 0);

    ::kill (static_cast<pid_t> (first), SIGKILL);
    REQUIRE (waitUntil ([&harness] { return ! harness->isSidecarRunning(); }));

    REQUIRE (harness->configure ("gpt-5.6-terra", Json::object()).succeeded);

    const auto second = harness->sidecarProcessId().value_or (0);
    REQUIRE (second != 0);
    REQUIRE (second != first);
}

TEST_CASE ("shutdown ends the sidecar, and shutting down twice is harmless", "[collab]")
{
    const Harness harness;
    harness->start();

    REQUIRE (harness->configure ("gpt-5.6-terra", Json::object()).succeeded);

    const auto processId = harness->sidecarProcessId().value_or (0);
    REQUIRE (processId != 0);

    REQUIRE (harness->shutdownSidecar().succeeded);
    REQUIRE_FALSE (harness->isSidecarRunning());
    REQUIRE (waitUntil ([processId] { return ! processExists (processId); }));

    // Nothing is running, so nothing is started in order to be shut down.
    REQUIRE (harness->shutdownSidecar().succeeded);
    REQUIRE_FALSE (harness->isSidecarRunning());
    REQUIRE_FALSE (harness->sidecarProcessId().has_value());
}

TEST_CASE ("the service going away takes the sidecar and the socket with it", "[collab]")
{
    Harness harness;
    harness->start();

    REQUIRE (harness->configure ("gpt-5.6-terra", Json::object()).succeeded);

    const auto processId = harness->sidecarProcessId().value_or (0);
    REQUIRE (processId != 0);
    REQUIRE (std::filesystem::exists (harness.socketPath()));

    harness.release();

    REQUIRE_FALSE (processExists (processId));
    REQUIRE_FALSE (std::filesystem::exists (harness.socketPath()));
}

TEST_CASE ("a second connection is refused and the connected sidecar is untouched", "[collab]")
{
    const Harness harness;
    harness->start();

    REQUIRE (harness->configure ("gpt-5.6-terra", Json::object()).succeeded);

    const int intruder = connectDirectly (harness.socketPath());
    REQUIRE (intruder >= 0);

    pollfd waiting { intruder, POLLIN, 0 };
    REQUIRE (::poll (&waiting, 1, 5000) > 0);

    std::array<char, 16> unread {};
    REQUIRE (::read (intruder, unread.data(), unread.size()) == 0);
    ::close (intruder);

    REQUIRE (harness->configure ("gpt-5.6-terra", Json::object()).succeeded);
    REQUIRE (harness->isSidecarRunning());
}

TEST_CASE ("the socket is serviced by the service's own thread", "[collab]")
{
    Harness harness { "two-in-one-write" };
    harness->start();

    REQUIRE (harness->configure ("gpt-5.6-terra", Json::object()).succeeded);
    REQUIRE (harness.waitForReports (2));

    const auto threads = harness.reportingThreads();

    REQUIRE (threads.size() == 2);

    for (const auto& thread : threads)
    {
        REQUIRE (thread == harness->serviceThreadId());
        REQUIRE (thread != std::this_thread::get_id());
    }
}
