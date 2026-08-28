#include "CollaboratorHarness.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using duet::collab::Json;
using duet::collab::OpeningContext;
using duet::collab::RunStatus;
using duet::collab::SelectionKind;
using duet::collab::ToolPhase;
using duet::testing::Harness;
using duet::testing::RecordingListener;
using namespace std::chrono_literals;

namespace
{
/** The payloads of the double's reports that carry that tag, in order. */
std::vector<Json> reportsTagged (const Harness& harness, const std::string& tag)
{
    std::vector<Json> chosen;

    for (const auto& report : harness.reports())
        if (report.at ("tag") == tag)
            chosen.push_back (report.at ("payload"));

    return chosen;
}

/** The opening context of a producer who had two clips selected, at bar 9 beat
    2.5, with the transport running.
*/
OpeningContext clipsAtBarNine()
{
    OpeningContext context;
    context.selection = SelectionKind::clips;
    context.selectionIds = { "clip-3", "clip-4" };
    context.playheadBar = 9;
    context.playheadBeat = 2.5;
    context.transportPlaying = true;

    return context;
}
} // namespace

TEST_CASE ("a run carries the prompt and the opening context, and ends later", "[collab]")
{
    RecordingListener listener;
    Harness harness { "run-echo" };
    harness->setTaskRunListener (&listener);
    harness->start();

    const auto start = harness->startRun ("something's off in the drop", clipsAtBarNine());

    REQUIRE (start.started);
    REQUIRE_FALSE (start.runId.empty());

    REQUIRE (harness.waitForReports (1));

    const auto sent = harness.reports().at (0).at ("payload");

    REQUIRE (sent.at ("runId") == start.runId);
    REQUIRE (sent.at ("prompt") == "something's off in the drop");

    const auto& opening = sent.at ("openingContext");

    REQUIRE (opening.at ("selection").at ("kind") == "clips");
    REQUIRE (opening.at ("selection").at ("ids") == Json::array ({ "clip-3", "clip-4" }));
    REQUIRE (opening.at ("playhead").at ("bar") == 9);
    REQUIRE (opening.at ("playhead").at ("beat") == 2.5);
    REQUIRE (opening.at ("transportPlaying") == true);

    // Completion was not the answer to the call: it arrives as a terminal event.
    REQUIRE (listener.waitForTerminals (1));

    const auto terminals = listener.of (RecordingListener::Kind::terminal);

    REQUIRE (terminals.size() == 1);
    REQUIRE (terminals.at (0).runId == start.runId);
    REQUIRE (terminals.at (0).status == RunStatus::completed);
    REQUIRE (terminals.at (0).error.empty());
}

TEST_CASE ("commentary deltas reach the listener in order", "[collab]")
{
    RecordingListener listener;
    const Harness harness { "run-stream" };
    harness->setTaskRunListener (&listener);
    harness->start();

    const auto start = harness->startRun ("what is off?", duet::collab::OpeningContext {});

    REQUIRE (start.started);
    REQUIRE (listener.waitForTerminals (1));

    const auto deltas = listener.of (RecordingListener::Kind::commentary);

    REQUIRE (deltas.size() == 3);
    REQUIRE (deltas.at (0).text == "Something");
    REQUIRE (deltas.at (1).text == " is off");
    REQUIRE (deltas.at (2).text == " in the drop.");

    for (const auto& delta : deltas)
        REQUIRE (delta.runId == start.runId);

    // The whole commentary is the deltas, concatenated as they arrived.
    REQUIRE (listener.commentary() == "Something is off in the drop.");
}

TEST_CASE ("tool activity reaches the listener named, and in the order sent", "[collab]")
{
    RecordingListener listener;
    const Harness harness { "run-tools" };
    harness->setTaskRunListener (&listener);
    harness->start();

    const auto start = harness->startRun ("what is in this project?", OpeningContext {});

    REQUIRE (start.started);
    REQUIRE (listener.waitForTerminals (1));

    const auto activity = listener.of (RecordingListener::Kind::tool);

    REQUIRE (activity.size() == 4);
    REQUIRE (activity.at (0).text == "list_tracks");
    REQUIRE (activity.at (0).phase == ToolPhase::start);
    REQUIRE (activity.at (1).text == "list_tracks");
    REQUIRE (activity.at (1).phase == ToolPhase::end);
    REQUIRE (activity.at (2).text == "get_arrangement");
    REQUIRE (activity.at (2).phase == ToolPhase::start);
    REQUIRE (activity.at (3).text == "get_arrangement");
    REQUIRE (activity.at (3).phase == ToolPhase::end);

    for (const auto& event : activity)
        REQUIRE (event.runId == start.runId);
}

TEST_CASE ("a second run is rejected while one is in progress, which is untouched", "[collab]")
{
    RecordingListener listener;
    const Harness harness { "run-slow-finish" };
    harness->setTaskRunListener (&listener);
    harness->start();

    const auto first = harness->startRun ("give me a turnaround into bar 9", OpeningContext {});

    REQUIRE (first.started);

    const auto second = harness->startRun ("and a fill before it", OpeningContext {});

    REQUIRE_FALSE (second.started);
    REQUIRE (second.runId.empty());
    REQUIRE (second.error.code == duet::collab::rpcError::runAlreadyActive);

    // The run in progress is still the first one, and it ends as it would have.
    REQUIRE (harness->activeRunId() == first.runId);
    REQUIRE (listener.waitForTerminals (1));

    const auto terminals = listener.of (RecordingListener::Kind::terminal);

    REQUIRE (terminals.size() == 1);
    REQUIRE (terminals.at (0).runId == first.runId);
    REQUIRE (terminals.at (0).status == RunStatus::completed);
    REQUIRE_FALSE (harness->activeRunId().has_value());
}

TEST_CASE ("cancel ends the run once, and cancelling again is harmless", "[collab]")
{
    RecordingListener listener;
    Harness harness { "run-hang-once" };
    harness->setTaskRunListener (&listener);
    harness->start();

    const auto start = harness->startRun ("try something", OpeningContext {});

    REQUIRE (start.started);

    // Cancel a run the sidecar is actually working on, not one still in flight.
    REQUIRE (harness.waitForReports (1));
    REQUIRE (harness.reports().at (0).at ("tag") == "run.start");

    REQUIRE (harness->cancelRun (start.runId));
    REQUIRE (listener.waitForTerminals (1));

    const auto terminals = listener.of (RecordingListener::Kind::terminal);

    REQUIRE (terminals.size() == 1);
    REQUIRE (terminals.at (0).runId == start.runId);
    REQUIRE (terminals.at (0).status == RunStatus::canceled);

    // The sidecar was told to stop, which is what makes the teardown more than
    // the DAW looking away.
    REQUIRE (harness.waitForReports (2));
    REQUIRE (harness.reports().at (1).at ("tag") == "run.cancel");
    REQUIRE (harness.reports().at (1).at ("payload").at ("runId") == start.runId);

    // A second cancel of the same run finds nothing to cancel.
    REQUIRE_FALSE (harness->cancelRun (start.runId));
    std::this_thread::sleep_for (250ms);
    REQUIRE (listener.of (RecordingListener::Kind::terminal).size() == 1);
}

TEST_CASE ("a failed run carries the sidecar's error, and the next run is accepted", "[collab]")
{
    RecordingListener listener;
    const Harness harness { "run-fail" };
    harness->setTaskRunListener (&listener);
    harness->start();

    const auto first = harness->startRun ("what is the key?", OpeningContext {});

    REQUIRE (first.started);
    REQUIRE (listener.waitForTerminals (1));

    const auto failure = listener.of (RecordingListener::Kind::terminal).at (0);

    REQUIRE (failure.runId == first.runId);
    REQUIRE (failure.status == RunStatus::failed);
    REQUIRE (failure.error == "the provider refused the request");

    // Nothing is held over from a failure: the service takes the next run at once.
    REQUIRE_FALSE (harness->activeRunId().has_value());

    const auto second = harness->startRun ("try again", OpeningContext {});

    REQUIRE (second.started);
    REQUIRE (second.runId != first.runId);
    REQUIRE (listener.waitForTerminals (2));

    const auto terminals = listener.of (RecordingListener::Kind::terminal);

    REQUIRE (terminals.size() == 2);
    REQUIRE (terminals.at (1).runId == second.runId);
    REQUIRE (terminals.at (1).status == RunStatus::failed);
}

TEST_CASE ("a sidecar that dies mid-run fails that run, and the next spawns a fresh one",
           "[collab]")
{
    RecordingListener listener;
    const Harness harness { "run-die" };
    harness->setTaskRunListener (&listener);
    harness->start();

    const auto first = harness->startRun ("look at the drop", OpeningContext {});

    REQUIRE (first.started);
    REQUIRE (listener.waitForTerminals (1));

    const auto died = listener.of (RecordingListener::Kind::terminal).at (0);

    REQUIRE (died.runId == first.runId);
    REQUIRE (died.status == RunStatus::failed);
    REQUIRE (died.error == duet::collab::backendUnavailableMessage);
    REQUIRE_FALSE (harness->activeRunId().has_value());

    const auto second = harness->startRun ("look again", OpeningContext {});

    REQUIRE (second.started);
    REQUIRE (listener.waitForTerminals (2));

    // Two sidecars, not one: the second run got a process of its own.
    const auto lives = reportsTagged (harness, "alive");

    REQUIRE (lives.size() == 2);
    REQUIRE (lives.at (0).at ("pid") != lives.at (1).at ("pid"));

    // Nothing queued and nothing retried: two runs asked for, two runs sent, two
    // runs ended.
    REQUIRE (reportsTagged (harness, "run.start").size() == 2);
    REQUIRE (listener.of (RecordingListener::Kind::terminal).size() == 2);
}

TEST_CASE ("events naming an unknown or finished run are ignored", "[collab]")
{
    RecordingListener listener;
    const Harness harness { "run-noise" };
    harness->setTaskRunListener (&listener);
    harness->start();

    const auto start = harness->startRun ("say something", OpeningContext {});

    REQUIRE (start.started);
    REQUIRE (listener.waitForTerminals (1));

    // Long enough for a second ending to have arrived, had one been let through.
    std::this_thread::sleep_for (250ms);

    const auto terminals = listener.of (RecordingListener::Kind::terminal);

    REQUIRE (terminals.size() == 1);
    REQUIRE (terminals.at (0).runId == start.runId);
    REQUIRE (terminals.at (0).status == RunStatus::completed);

    // Only the run that existed, and only while it did.
    REQUIRE (listener.commentary() == "the real delta");

    for (const auto& event : listener.events())
        REQUIRE (event.runId == start.runId);
}

TEST_CASE ("starting, cancelling and receiving never wait on the sidecar", "[collab]")
{
    RecordingListener listener;
    Harness harness { "run-slow-hang" };
    harness->setTaskRunListener (&listener);
    harness->start();

    // The sidecar takes 600 ms to call home and 500 ms to answer anything, so a
    // call that waited on it could not come back inside this.
    constexpr auto promptly = 200ms;

    const auto beforeStart = std::chrono::steady_clock::now();
    const auto start = harness->startRun ("is the mix too wide?", OpeningContext {});
    const auto startTook = std::chrono::steady_clock::now() - beforeStart;

    REQUIRE (start.started);
    REQUIRE (startTook < promptly);

    // Wait until the sidecar really has the run, so that the cancel below is one
    // it has to be told about.
    REQUIRE (harness.waitForReports (1));
    REQUIRE (harness.reports().at (0).at ("tag") == "run.start");

    const auto beforeCancel = std::chrono::steady_clock::now();
    const auto canceled = harness->cancelRun (start.runId);
    const auto cancelTook = std::chrono::steady_clock::now() - beforeCancel;

    REQUIRE (canceled);
    REQUIRE (cancelTook < promptly);

    REQUIRE (listener.waitForTerminals (1));

    const auto events = listener.events();

    REQUIRE_FALSE (events.empty());

    // And nothing was received on a DAW thread: the socket is the service's own.
    for (const auto& event : events)
    {
        REQUIRE (event.thread == harness->serviceThreadId());
        REQUIRE (event.thread != std::this_thread::get_id());
    }
}

TEST_CASE ("the run after a canceled one starts as cleanly as the first", "[collab]")
{
    RecordingListener listener;
    Harness harness { "run-hang-once" };
    harness->setTaskRunListener (&listener);
    harness->start();

    const auto first = harness->startRun ("something's off in the drop", clipsAtBarNine());

    REQUIRE (first.started);
    REQUIRE (harness.waitForReports (1));
    REQUIRE (harness->cancelRun (first.runId));
    REQUIRE (listener.waitForTerminals (1));
    REQUIRE (listener.of (RecordingListener::Kind::terminal).at (0).status == RunStatus::canceled);

    // Nothing of the canceled run is left to get in the way of the next one.
    REQUIRE_FALSE (harness->activeRunId().has_value());

    const auto second = harness->startRun ("try the turnaround instead", OpeningContext {});

    REQUIRE (second.started);
    REQUIRE (second.runId != first.runId);
    REQUIRE (listener.waitForTerminals (2));

    const auto terminals = listener.of (RecordingListener::Kind::terminal);

    REQUIRE (terminals.size() == 2);
    REQUIRE (terminals.at (1).runId == second.runId);
    REQUIRE (terminals.at (1).status == RunStatus::completed);

    // The second run streamed, and every word of it is the second run's own.
    const auto deltas = listener.of (RecordingListener::Kind::commentary);

    REQUIRE (deltas.size() == 1);
    REQUIRE (deltas.at (0).runId == second.runId);
    REQUIRE (listener.commentary() == "the second run");
    REQUIRE_FALSE (harness->activeRunId().has_value());
}

TEST_CASE ("a refused run.start fails the run with what the sidecar said", "[collab]")
{
    RecordingListener listener;
    const Harness harness { "run-refuse" };
    harness->setTaskRunListener (&listener);
    harness->start();

    const auto first = harness->startRun ("what is the key?", OpeningContext {});

    REQUIRE (first.started);
    REQUIRE (listener.waitForTerminals (1));

    const auto refused = listener.of (RecordingListener::Kind::terminal).at (0);

    REQUIRE (refused.runId == first.runId);
    REQUIRE (refused.status == RunStatus::failed);
    REQUIRE (refused.error == "no provider is configured");

    // Fast, and over: the next run is taken straight away.
    REQUIRE_FALSE (harness->activeRunId().has_value());
    REQUIRE (harness->startRun ("try again", OpeningContext {}).started);
}

TEST_CASE ("a sidecar that cannot be started fails the run rather than hanging", "[collab]")
{
    RecordingListener listener;
    const Harness harness { "obey", "/nonexistent/duet-sidecar" };
    harness->setTaskRunListener (&listener);
    harness->start();

    const auto start = harness->startRun ("anything at all", OpeningContext {});

    REQUIRE (start.started);
    REQUIRE (listener.waitForTerminals (1));

    const auto unreachable = listener.of (RecordingListener::Kind::terminal).at (0);

    REQUIRE (unreachable.runId == start.runId);
    REQUIRE (unreachable.status == RunStatus::failed);
    REQUIRE (unreachable.error == duet::collab::backendUnavailableMessage);
    REQUIRE_FALSE (harness->activeRunId().has_value());
}
