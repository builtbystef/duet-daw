#pragma once

/** One Task Run's worth of tool calls, driven through the socket protocol
    against a real project.

    The seam the spec names for this area is the protocol, so a tool assertion
    is made on what came back over a socket from a real child process — never on
    a C++ call into the tool layer. What the double asked, the service answered,
    and the project it answered from are all real; only the model asking the
    questions is not there.
*/

#include "CollaboratorHarness.h"

#include <duet/collab/ProjectTools.h>
#include <duet/collab/SuggestTool.h>
#include <duet/collab/ToolDispatch.h>
#include <duet/collab/TrackAnalysis.h>
#include <duet/model/Session.h>
#include <duet/testing/TestSupport.h>

#include <cstddef>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace duet::testing
{
/** One entry of the list the `call-tools` script is given. */
inline Json toolCall (const std::string& tool, Json arguments = Json::object())
{
    Json call = Json::object();
    call["tool"] = tool;
    call["args"] = std::move (arguments);

    return call;
}

/** One entry of that same list that says something rather than asking: what the
    Collaborator writes in the conversation while it works.
*/
inline Json toolCommentary (const std::string& text)
{
    Json entry = Json::object();
    entry["text"] = text;

    return entry;
}

/** One element of a `suggest` call: what the change is, and what it does. */
inline Json suggestElement (const std::string& description, Json operations)
{
    Json element = Json::object();
    element["description"] = description;
    element["operations"] = std::move (operations);

    return element;
}

/** A `suggest` call carrying a summary and its elements. */
inline Json suggestCall (const std::string& summary, Json elements)
{
    Json arguments = Json::object();
    arguments["summary"] = summary;
    arguments["elements"] = std::move (elements);

    return toolCall ("suggest", arguments);
}

/** A call naming one track, which is what three of the five tools take. */
inline Json toolCall (const std::string& tool, duet::model::TrackRef track)
{
    Json arguments = Json::object();
    arguments["trackId"] = duet::collab::toolId::forTrack (track);

    return toolCall (tool, arguments);
}

/** Only what a tool run needs of a listener: when the run ended, and how.

    Declared before the harness that reports to it, which is what the seam asks
    of anyone who attaches — this class holds both, in that order, so a test
    cannot get it wrong.
*/
class RunEnding final : public duet::collab::TaskRunListener
{
public:
    void commentaryDelta (const std::string& /*runId*/, const std::string& delta) override
    {
        const std::lock_guard lock (mutex);
        said += delta;
    }

    void toolActivity (const std::string& /*runId*/,
                       const std::string& /*tool*/,
                       duet::collab::ToolPhase /*phase*/) override
    {
    }

    void runFinished (const std::string& /*runId*/,
                      duet::collab::RunStatus status,
                      const std::string& error) override
    {
        const std::lock_guard lock (mutex);
        ended = true;
        howItEnded = status;
        why = error;
    }

    [[nodiscard]] bool hasEnded() const
    {
        const std::lock_guard lock (mutex);

        return ended;
    }

    [[nodiscard]] duet::collab::RunStatus status() const
    {
        const std::lock_guard lock (mutex);

        return howItEnded;
    }

    [[nodiscard]] std::string error() const
    {
        const std::lock_guard lock (mutex);

        return why;
    }

    /** Everything the run said, its deltas in the order they arrived. */
    [[nodiscard]] std::string commentary() const
    {
        const std::lock_guard lock (mutex);

        return said;
    }

private:
    mutable std::mutex mutex;
    bool ended = false;
    duet::collab::RunStatus howItEnded = duet::collab::RunStatus::failed;
    std::string why;
    std::string said;
};

/** How long a tool run is given to finish: long enough to spawn a child process
    and answer a corpus fixture's worth of calls on a loaded machine.
*/
inline constexpr int toolRunTimeoutMs = 20000;

/** What a tool run may be given besides the calls to make. */
struct ToolRunOptions
{
    duet::collab::ProjectReadMarshal marshal = messageThreadMarshal();

    /** The measured-analysis tool this run answers `get_track_analysis` with,
        and nothing when the run is about the read tools alone.

        The run borrows it rather than making one, because what that tool keeps
        between calls — the render of each track — is the thing a test about
        caching is about, and a tool that died with the run would have nothing
        to keep.
    */
    duet::collab::TrackAnalysis* measured = nullptr;

    /** Run on the message thread each time the wait for the run pumps it: what
        a producer does while a call is in flight.
    */
    std::function<void()> meanwhile;
};

class ToolRun
{
public:
    /** Runs every call in order and waits for the run to end.

        The wait pumps the message loop, and it has to: every tool read is
        marshalled onto the message thread, so a test that blocked here instead
        would be the thread the answer is waiting for.
    */
    ToolRun (duet::model::Session& session,
             const Json& calls,
             duet::collab::ProjectReadMarshal marshal = messageThreadMarshal())
        : ToolRun (session, calls, ToolRunOptions { std::move (marshal), nullptr, {} })
    {
    }

    ToolRun (duet::model::Session& session, const Json& calls, ToolRunOptions options)
        : tools (session, options.marshal), writes (session, options.marshal),
          harness ("call-tools", std::vector<std::string> { calls.dump() })
    {
        tools.addTo (registry);
        writes.addTo (registry);

        if (options.measured != nullptr)
            options.measured->addTo (registry);

        harness->setMethodHandler ("tool.call",
                                   [this] (const Json& params) { return registry.call (params); });
        harness->setTaskRunListener (&ending);
        harness->start();

        const auto start = harness->startRun ("what is in this project?", {});
        runId = start.runId;
        accepted = start.started;

        ran = pumpUntil (
            [&]
            {
                if (options.meanwhile)
                    options.meanwhile();

                return ending.hasEnded();
            },
            toolRunTimeoutMs);

        for (const auto& report : harness.reports())
            if (report.at ("tag") == "tool")
                answers.push_back (report.at ("payload").at ("response"));
    }

    /** Whether the run was accepted and reached its ending. */
    [[nodiscard]] bool finished() const
    {
        return accepted && ran && ending.status() == duet::collab::RunStatus::completed;
    }

    /** The whole JSON-RPC response to each call, in the order they were made.

        A reference and not a copy, deliberately: an assertion reaches into a
        result, and a result handed over by value dies at the end of the
        expression that asked for it, taking the reference with it.
    */
    [[nodiscard]] const std::vector<Json>& responses() const { return answers; }

    /** What one call answered with, and an empty object when it failed. */
    [[nodiscard]] const Json& result (std::size_t call) const { return memberOf (call, "result"); }

    /** Why one call failed, and an empty object when it did not. */
    [[nodiscard]] const Json& error (std::size_t call) const { return memberOf (call, "error"); }

    [[nodiscard]] const std::string& id() const { return runId; }

    /** Everything the run said while it worked. */
    [[nodiscard]] std::string commentary() const { return ending.commentary(); }

    /** The Suggestions this run's calls made, oldest first. */
    [[nodiscard]] std::vector<duet::collab::Suggestion> suggestions() const
    {
        return writes.suggestions();
    }

    /** The one Suggestion a call answered with, read back from the tool.

        A call that made none reads back as a Suggestion of nothing, whose own
        id is empty — the shape the model facade already answers an unknown
        reference in, so an assertion says what it means without unwrapping.
    */
    [[nodiscard]] duet::collab::Suggestion suggestion (std::size_t call) const
    {
        const auto& answer = result (call);

        if (! answer.contains ("suggestionId"))
            return { {}, {} };

        return writes.suggestion (answer.at ("suggestionId").get<std::string>())
            .value_or (duet::collab::Suggestion { {}, {} });
    }

    /** The thread the service answers tool calls on, which is never the thread
        a project read happens on.
    */
    [[nodiscard]] std::thread::id serviceThread() const { return harness->serviceThreadId(); }

private:
    [[nodiscard]] const Json& memberOf (std::size_t call, const char* member) const
    {
        static const Json nothing = Json::object();

        if (call >= answers.size() || ! answers.at (call).contains (member))
            return nothing;

        return answers.at (call).at (member);
    }

    duet::collab::ToolRegistry registry;
    duet::collab::ProjectTools tools;
    duet::collab::SuggestTool writes;

    // Before the harness, so that the service is gone before the listener is.
    RunEnding ending;
    Harness harness;

    std::vector<Json> answers;
    std::string runId;
    bool accepted = false;
    bool ran = false;
};

/** The one entry of a track list that names this track. */
inline Json trackEntry (const Json& listTracks, duet::model::TrackRef track)
{
    const auto wanted = duet::collab::toolId::forTrack (track);

    for (const auto& entry : listTracks.at ("tracks"))
        if (entry.at ("id") == wanted)
            return entry;

    return Json::object();
}
} // namespace duet::testing
