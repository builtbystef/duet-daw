#pragma once

/** What both Collaborator suites drive: a real service, a real socket, and the
    test-double sidecar as a real child process.

    It lives in a header rather than in one suite because the seam the spec names
    is the socket protocol, and every slice of that protocol is asserted through
    the same three things.
*/

#include <duet/collab/CollaboratorService.h>
#include <duet/collab/TaskRun.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace duet::testing
{
using duet::collab::CollaboratorService;
using duet::collab::Json;
using duet::collab::RpcOutcome;
using duet::collab::RunStatus;
using duet::collab::ToolPhase;
using namespace std::chrono_literals;

/** A folder under the system temp directory to put a socket in, removed with
    this object. The socket path is short on purpose: `sun_path` is 108 bytes.
*/
class TempSocketFolder
{
public:
    TempSocketFolder()
    {
        static int counter = 0;

        folder =
            std::filesystem::temp_directory_path()
            / ("duet-collab-" + std::to_string (::getpid()) + "-" + std::to_string (++counter));

        std::filesystem::create_directories (folder);
    }

    ~TempSocketFolder()
    {
        std::error_code ignored;
        std::filesystem::remove_all (folder, ignored);
    }

    TempSocketFolder (const TempSocketFolder&) = delete;
    TempSocketFolder& operator= (const TempSocketFolder&) = delete;

    [[nodiscard]] std::filesystem::path socketPath() const { return folder / "sidecar.sock"; }

private:
    std::filesystem::path folder;
};

/** A real service pointed at the test-double sidecar, and somewhere for the
    double's observations to land.

    The double sends what it saw back over the same connection, as `test.report`
    requests, so an assertion about a response the double received is made in
    this process instead of by reading the child's output.
*/
class Harness
{
public:
    /** `executable` is the double unless a test wants a sidecar that is not
        there, which is the only way to drive a spawn that cannot succeed.
    */
    explicit Harness (const std::string& script = "obey",
                      const std::filesystem::path& executable = DUET_SIDECAR_DOUBLE)
        : Harness (script, {}, executable)
    {
    }

    /** The same, for a script that takes an argument of its own. */
    Harness (const std::string& script,
             const std::vector<std::string>& scriptArguments,
             const std::filesystem::path& executable = DUET_SIDECAR_DOUBLE)
        : Harness (argumentsOf (script, scriptArguments), executable)
    {
    }

    /** A sidecar launched with a list of arguments rather than a script name,
        which is what the real one takes.
    */
    Harness (const std::vector<std::string>& arguments, const std::filesystem::path& executable)
    {
        CollaboratorService::Configuration configuration;
        configuration.socketPath = folder.socketPath();
        configuration.sidecar.executable = executable;
        configuration.sidecar.arguments = arguments;
        configuration.requestTimeout = 5s;
        configuration.sidecarStartTimeout = 5s;
        configuration.sidecarStopTimeout = 2s;

        service = std::make_unique<CollaboratorService> (configuration);

        service->setMethodHandler ("test.report",
                                   [this] (const Json& params)
                                   {
                                       const std::lock_guard lock (mutex);
                                       received.push_back (params);
                                       handlerThreads.push_back (std::this_thread::get_id());
                                       arrived.notify_all();

                                       return RpcOutcome::success (Json::object());
                                   });
    }

    CollaboratorService& operator*() const { return *service; }
    CollaboratorService* operator->() const { return service.get(); }

    [[nodiscard]] std::filesystem::path socketPath() const { return folder.socketPath(); }

    /** Waits for that many reports to have arrived. */
    bool waitForReports (std::size_t count, std::chrono::milliseconds timeout = 5s)
    {
        std::unique_lock lock (mutex);

        return arrived.wait_for (lock, timeout, [this, count] { return received.size() >= count; });
    }

    [[nodiscard]] std::vector<Json> reports() const
    {
        const std::lock_guard lock (mutex);

        return received;
    }

    [[nodiscard]] std::vector<std::thread::id> reportingThreads() const
    {
        const std::lock_guard lock (mutex);

        return handlerThreads;
    }

    void release() { service.reset(); }

private:
    /** A script name and its own arguments, as one argument list. */
    static std::vector<std::string> argumentsOf (const std::string& script,
                                                 const std::vector<std::string>& scriptArguments)
    {
        std::vector<std::string> arguments { script };
        arguments.insert (arguments.end(), scriptArguments.begin(), scriptArguments.end());

        return arguments;
    }

    TempSocketFolder folder;
    std::unique_ptr<CollaboratorService> service;

    mutable std::mutex mutex;
    std::condition_variable arrived;
    std::vector<Json> received;
    std::vector<std::thread::id> handlerThreads;
};

/** Everything a Task Run told the DAW, in the order it was told.

    A listener and not a poll: the seam's promise is that a run's commentary,
    tool activity and ending arrive as calls, in order, so the assertions are
    made on the record of those calls and on nothing else.

    Every test declares one of these before its harness, which is what the seam
    asks of anyone who attaches: a listener outlives the service that reports to
    it, or is cleared before that service goes.
*/
class RecordingListener final : public duet::collab::TaskRunListener
{
public:
    enum class Kind : std::uint8_t
    {
        commentary,
        tool,
        terminal
    };

    struct Event
    {
        Kind kind = Kind::commentary;
        std::string runId;

        /** The delta, on commentary; the tool's name, on tool activity. */
        std::string text;

        ToolPhase phase = ToolPhase::start;
        RunStatus status = RunStatus::completed;
        std::string error;
        std::thread::id thread;
    };

    void commentaryDelta (const std::string& runId, const std::string& delta) override
    {
        Event event;
        event.kind = Kind::commentary;
        event.runId = runId;
        event.text = delta;
        record (event);
    }

    void toolActivity (const std::string& runId, const std::string& tool, ToolPhase phase) override
    {
        Event event;
        event.kind = Kind::tool;
        event.runId = runId;
        event.text = tool;
        event.phase = phase;
        record (event);
    }

    void runFinished (const std::string& runId, RunStatus status, const std::string& error) override
    {
        Event event;
        event.kind = Kind::terminal;
        event.runId = runId;
        event.status = status;
        event.error = error;
        record (event);
    }

    [[nodiscard]] std::vector<Event> events() const
    {
        const std::lock_guard lock (mutex);

        return seen;
    }

    [[nodiscard]] std::vector<Event> of (Kind kind) const
    {
        std::vector<Event> chosen;

        for (const auto& event : events())
            if (event.kind == kind)
                chosen.push_back (event);

        return chosen;
    }

    /** The run's commentary: its deltas, concatenated in the order they came. */
    [[nodiscard]] std::string commentary() const
    {
        std::string whole;

        for (const auto& event : of (Kind::commentary))
            whole += event.text;

        return whole;
    }

    /** Waits for that many terminal events, and says whether they arrived. */
    bool waitForTerminals (std::size_t count, std::chrono::milliseconds timeout = 5s)
    {
        std::unique_lock lock (mutex);

        return arrived.wait_for (lock,
                                 timeout,
                                 [this, count]
                                 {
                                     std::size_t terminals = 0;

                                     for (const auto& event : seen)
                                         if (event.kind == Kind::terminal)
                                             ++terminals;

                                     return terminals >= count;
                                 });
    }

private:
    void record (const Event& event)
    {
        const std::lock_guard lock (mutex);
        auto stamped = event;
        stamped.thread = std::this_thread::get_id();
        seen.push_back (stamped);
        arrived.notify_all();
    }

    mutable std::mutex mutex;
    std::condition_variable arrived;
    std::vector<Event> seen;
};

/** Polls until something is true, or gives up. */
inline bool waitUntil (const std::function<bool()>& settled, std::chrono::milliseconds timeout = 5s)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline)
    {
        if (settled())
            return true;

        std::this_thread::sleep_for (10ms);
    }

    return settled();
}

/** Whether a process is still there to be signalled. */
inline bool processExists (int processId)
{
    return ::kill (static_cast<pid_t> (processId), 0) == 0;
}

/** A second client on the socket, connected the way any sidecar would. */
inline int connectDirectly (const std::filesystem::path& path)
{
    const auto text = path.string();

    sockaddr_un address {};
    address.sun_family = AF_UNIX;
    std::memcpy (std::data (address.sun_path), text.data(), text.size());

    const int descriptor = ::socket (AF_UNIX, SOCK_STREAM, 0);

    if (descriptor < 0)
        return -1;

    // The sockets API is typed on sockaddr.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto* generic = reinterpret_cast<const sockaddr*> (&address);

    if (::connect (descriptor, generic, sizeof (address)) != 0)
    {
        ::close (descriptor);

        return -1;
    }

    return descriptor;
}
} // namespace duet::testing
