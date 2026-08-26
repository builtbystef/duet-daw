// A sidecar that speaks the protocol and nothing else: no Node, no LLM, no UI.
//
// This is the harness the Collaborator service is verified against, and it is
// also the escape-hatch demonstration the spec asks for — anything that connects
// to the socket, frames its messages with newlines and answers JSON-RPC 2.0 can
// take the real sidecar's place. Nothing here shares a line of code with the
// service it talks to, so a test that passes says the protocol works and not
// that one implementation agrees with itself.
//
// Run as: duet_sidecar_double <socket-path> [script]
//
// The script runs once on connecting, and may leave writes for later, released
// by a chosen inbound request. Otherwise the double obeys — every request gets
// an empty result, and `shutdown` gets one and then ends the process.
//
// The `run-*` scripts are the Task Run ones: each says what this double does
// when a `run.start` arrives, which is the only way the lifecycle rules can be
// driven from the far side of the seam.

#include <nlohmann/json.hpp>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
using Json = nlohmann::json;

constexpr auto connectRetryInterval = std::chrono::milliseconds { 20 };
constexpr int connectAttempts = 250;

/** How long the `run-slow-hang` script takes to call home, and then to answer
    anything about a run.

    Both are far longer than a call that waited on them could hide behind, which
    is what makes "the DAW never blocks on a run" assertable rather than assumed.
*/
constexpr auto slowConnectDelay = std::chrono::milliseconds { 600 };
constexpr auto slowReplyDelay = std::chrono::milliseconds { 500 };

int connectTo (const std::string& path)
{
    sockaddr_un address {};
    address.sun_family = AF_UNIX;

    if (path.size() + 1 > sizeof (address.sun_path))
        return -1;

    std::memcpy (std::data (address.sun_path), path.data(), path.size());

    for (int attempt = 0; attempt < connectAttempts; ++attempt)
    {
        const int socketDescriptor = ::socket (AF_UNIX, SOCK_STREAM, 0);

        if (socketDescriptor >= 0)
        {
            // The sockets API is typed on sockaddr.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            const auto* generic = reinterpret_cast<const sockaddr*> (&address);

            if (::connect (socketDescriptor, generic, sizeof (address)) == 0)
                return socketDescriptor;

            ::close (socketDescriptor);
        }

        std::this_thread::sleep_for (connectRetryInterval);
    }

    return -1;
}

/** The double's own framing, written out rather than shared with the service. */
class Connection
{
public:
    explicit Connection (int descriptorToUse) : descriptor (descriptorToUse) {}

    ~Connection() { ::close (descriptor); }

    Connection (const Connection&) = delete;
    Connection& operator= (const Connection&) = delete;

    void writeRaw (const std::string& bytes) const
    {
        auto remaining = std::string_view { bytes };

        while (! remaining.empty())
        {
            const auto count =
                ::send (descriptor, remaining.data(), remaining.size(), MSG_NOSIGNAL);

            if (count <= 0)
                return;

            remaining.remove_prefix (static_cast<std::size_t> (count));
        }
    }

    void send (const Json& message) const { writeRaw (message.dump() + "\n"); }

    /** The next whole message, or nothing when the connection has closed. */
    std::optional<Json> receive()
    {
        while (true)
        {
            const auto newline = buffer.find ('\n');

            if (newline != std::string::npos)
            {
                const auto line = buffer.substr (0, newline);
                buffer.erase (0, newline + 1);

                return Json::parse (line, nullptr, false);
            }

            std::vector<char> chunk (4096);
            const auto count = ::read (descriptor, chunk.data(), chunk.size());

            if (count <= 0)
                return std::nullopt;

            buffer.append (chunk.data(), static_cast<std::size_t> (count));
        }
    }

private:
    int descriptor;
    std::string buffer;
};

/** The double's own request ids, which nothing on the DAW side reads. */
int nextReportId()
{
    static int next = 900;

    return ++next;
}

Json request (int id, const std::string& method, const Json& params)
{
    return Json { { "jsonrpc", "2.0" }, { "id", id }, { "method", method }, { "params", params } };
}

/** Refuses a request the way a sidecar with no provider configured would. */
void replyWithError (const Connection& connection, const Json& incoming, const std::string& text)
{
    if (! incoming.contains ("id") || incoming["id"].is_null())
        return;

    connection.send (Json { { "jsonrpc", "2.0" },
                            { "id", incoming["id"] },
                            { "error", Json { { "code", -32603 }, { "message", text } } } });
}

void reply (const Connection& connection, const Json& incoming)
{
    if (! incoming.contains ("id") || incoming["id"].is_null())
        return;

    connection.send (
        Json { { "jsonrpc", "2.0" }, { "id", incoming["id"] }, { "result", Json::object() } });
}

/** Answers requests until a response arrives, and returns that response. */
std::optional<Json> awaitResponse (Connection& connection)
{
    while (auto message = connection.receive())
    {
        if (message->is_discarded() || ! message->is_object())
            continue;

        if (message->contains ("method"))
        {
            reply (connection, *message);
            continue;
        }

        return message;
    }

    return std::nullopt;
}

/** Sends one observation back to the DAW, for the test to assert on. */
void report (Connection& connection, int id, const std::string& tag, const Json& payload)
{
    connection.send (request (id, "test.report", Json { { "tag", tag }, { "payload", payload } }));
}

/** A notification about a run: no id, so no answer is expected. */
Json notification (const std::string& method, const Json& params)
{
    return Json { { "jsonrpc", "2.0" }, { "method", method }, { "params", params } };
}

void sendText (const Connection& connection, const std::string& runId, const std::string& delta)
{
    connection.send (notification ("run.text", Json { { "runId", runId }, { "delta", delta } }));
}

void sendToolActivity (const Connection& connection,
                       const std::string& runId,
                       const std::string& tool,
                       const std::string& phase)
{
    connection.send (notification (
        "run.toolActivity", Json { { "runId", runId }, { "tool", tool }, { "phase", phase } }));
}

void sendFinished (const Connection& connection,
                   const std::string& runId,
                   const std::string& status,
                   const std::string& error = {})
{
    Json params { { "runId", runId }, { "status", status } };

    if (! error.empty())
        params["error"] = error;

    connection.send (notification ("run.finished", params));
}

/** What this double does when a run starts, which is what each `run-*` script
    names. `runsSeen` counts the runs before this one, so a script can answer its
    first run differently from its second.
*/
void performRun (Connection& connection,
                 const std::string& script,
                 const Json& params,
                 int runsSeen)
{
    const auto runId = params.value ("runId", std::string {});

    if (script == "run-echo")
    {
        sendFinished (connection, runId, "completed");
        return;
    }

    if (script == "run-stream")
    {
        for (const auto& delta : { "Something", " is off", " in the drop." })
            sendText (connection, runId, delta);

        sendFinished (connection, runId, "completed");
        return;
    }

    if (script == "run-tools")
    {
        for (const auto& tool : { "list_tracks", "get_arrangement" })
        {
            sendToolActivity (connection, runId, tool, "start");
            sendToolActivity (connection, runId, tool, "end");
        }

        sendFinished (connection, runId, "completed");
        return;
    }

    if (script == "run-slow-finish")
    {
        // Long enough that a second run can be asked for while this one is on.
        std::this_thread::sleep_for (std::chrono::milliseconds { 400 });
        sendFinished (connection, runId, "completed");
        return;
    }

    if (script == "run-hang-once")
    {
        // The first run never ends of its own accord, so the only way out of it
        // is the producer's cancel; the second one behaves.
        if (runsSeen > 0)
        {
            sendText (connection, runId, "the second run");
            sendFinished (connection, runId, "completed");
        }

        return;
    }

    if (script == "run-fail")
    {
        // A provider that refused, reported the way a real one would be: the run
        // ends failed and the sidecar stays alive for the next one.
        sendFinished (connection, runId, "failed", "the provider refused the request");
        return;
    }

    if (script == "run-die")
    {
        // A sidecar that dies with a run in progress, without a word about it.
        std::_Exit (1);
    }

    if (script == "run-slow-hang")
    {
        // Slow at everything and finished with nothing: the run only ever ends
        // because the producer cancels it.
        return;
    }

    if (script == "run-refuse")
    {
        // Refused before it began, so nothing here ever runs.
        return;
    }

    if (script == "run-noise")
    {
        // A run that is not this one, a run that never existed, and words after
        // the ending — everything the DAW is supposed to throw away, around the
        // one delta and the one ending it is supposed to keep.
        sendText (connection, "run-bogus", "about nobody");
        sendFinished (connection, "run-bogus", "completed");
        sendText (connection, runId, "the real delta");
        sendFinished (connection, runId, "completed");
        sendText (connection, runId, "after the ending");
        sendFinished (connection, runId, "failed", "a second ending");
        return;
    }

    std::cerr << "duet_sidecar_double: script " << script << " has no run behaviour (" << runsSeen
              << " runs seen)\n";
}

/** Writes the script left for later: which inbound request releases them, and
    how long to wait between them.

    A message written in pieces cannot have anything else written between them —
    one stream, one message at a time — so the pieces go out after a reply and
    never between one and its request. Which reply is the test's to choose, so
    the test knows when the clock starts.
*/
struct Deferred
{
    std::vector<std::string> chunks;
    int afterRequests = 0;
    std::chrono::milliseconds gap { 0 };
};

Deferred runScript (Connection& connection, const std::string& script)
{
    if (script == "obey")
        return {};

    if (script == "two-in-one-write")
    {
        // Both messages in one write(), so the service sees them in one read.
        const auto first = request (
            901, "test.report", Json { { "tag", "first" }, { "payload", Json::object() } });
        const auto second = request (
            902, "test.report", Json { { "tag", "second" }, { "payload", Json::object() } });

        connection.writeRaw (first.dump() + "\n" + second.dump() + "\n");
        return {};
    }

    if (script == "escaped-newline")
    {
        report (connection, 903, "escaped", Json { { "text", "one\ntwo" } });
        return {};
    }

    if (script == "split-write")
    {
        const auto line =
            request (904, "test.report", Json { { "tag", "split" }, { "payload", Json::object() } })
                .dump();

        // Three writes, spaced out, with the newline last: for the length of
        // the gap the service is holding a message that is complete except for
        // its terminator, and must do nothing with it.
        const auto third = line.size() / 3;

        return { { line.substr (0, third), line.substr (third), "\n" },
                 1,
                 std::chrono::milliseconds { 400 } };
    }

    if (script == "malformed-line")
    {
        connection.writeRaw ("this is not JSON at all\n");
        const auto answer = awaitResponse (connection);
        report (connection, 905, "malformed", answer.value_or (Json()));
        return {};
    }

    if (script == "unknown-method")
    {
        connection.send (request (906, "no.such.method", Json::object()));
        const auto answer = awaitResponse (connection);
        report (connection, 907, "unknown", answer.value_or (Json()));
        return {};
    }

    if (script == "run-die")
    {
        // Which process this is, said before it has any reason to die, so that a
        // test can tell one sidecar from the one that replaced it.
        report (connection, nextReportId(), "alive", Json { { "pid", ::getpid() } });
        return {};
    }

    // A run script leaves nothing for the connect moment: what it does, it does
    // when a run starts.
    if (script.starts_with ("run-"))
        return {};

    std::cerr << "duet_sidecar_double: unknown script " << script << "\n";

    return {};
}
} // namespace

int main (int argc, char** argv)
try
{
    const std::span<char*> raw { argv, static_cast<std::size_t> (argc) };
    const std::vector<std::string> arguments (raw.begin(), raw.end());

    if (arguments.size() < 2)
    {
        std::cerr << "duet_sidecar_double: expected a socket path\n";
        return 2;
    }

    const auto script = arguments.size() > 2 ? arguments.at (2) : std::string { "obey" };

    if (script == "run-slow-hang")
        std::this_thread::sleep_for (slowConnectDelay);

    const int descriptor = connectTo (arguments.at (1));

    if (descriptor < 0)
    {
        std::cerr << "duet_sidecar_double: could not connect to " << arguments.at (1) << "\n";
        return 3;
    }

    Connection connection { descriptor };
    auto deferred = runScript (connection, script);

    int requestsSeen = 0;
    int runsSeen = 0;

    while (true)
    {
        const auto received = connection.receive();

        if (! received.has_value())
            break;

        const Json& message = *received;

        if (message.is_discarded() || ! message.is_object() || ! message.contains ("method"))
            continue;

        const auto method = message.value ("method", std::string {});

        if (script == "run-slow-hang" && (method == "run.start" || method == "run.cancel"))
            std::this_thread::sleep_for (slowReplyDelay);

        if (script == "run-refuse" && method == "run.start")
        {
            replyWithError (connection, message, "no provider is configured");
            continue;
        }

        ++requestsSeen;
        reply (connection, message);

        // Every run script reports what crossed the seam, so a test can assert on
        // it and can wait for the moment the sidecar has it.
        if (script.starts_with ("run-") && (method == "run.start" || method == "run.cancel"))
            report (connection, nextReportId(), method, message.value ("params", Json::object()));

        if (method == "run.start")
        {
            performRun (connection, script, message.value ("params", Json::object()), runsSeen);
            ++runsSeen;
        }

        if (! deferred.chunks.empty() && requestsSeen >= deferred.afterRequests)
        {
            for (const auto& chunk : deferred.chunks)
            {
                std::this_thread::sleep_for (deferred.gap);
                connection.writeRaw (chunk);
            }

            deferred.chunks.clear();
        }

        if (method == "shutdown")
            return 0;
    }

    return 0;
}
catch (const std::exception& failure)
{
    std::cerr << "duet_sidecar_double: " << failure.what() << "\n";

    return 4;
}
