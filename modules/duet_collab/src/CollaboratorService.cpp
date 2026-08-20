#include <duet/collab/CollaboratorService.h>

#include "LineReader.h"
#include "LocalSocketServer.h"
#include "SidecarProcess.h"

#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace duet::collab
{
namespace
{
    /** How long the service thread sleeps in poll() with nothing happening.

        Every request wakes it, so this paces one thing only: noticing a child
        process that died without closing the connection.
    */
    constexpr int pollIntervalMilliseconds = 50;

    constexpr std::size_t readChunkBytes = 4096;

    /** The result member, or the error member, of a response the sidecar sent. */
    RpcOutcome outcomeFrom (const Json& message)
    {
        if (! message.contains ("error") || message["error"].is_null())
            return RpcOutcome::success (message.value ("result", Json::object()));

        const auto& error = message["error"];

        if (! error.is_object())
            return RpcOutcome::failure (rpcError::internalError,
                                        "The sidecar sent a malformed error.");

        return RpcOutcome::failure (error.value ("code", rpcError::internalError),
                                    error.value ("message", std::string { "The sidecar failed." }));
    }
} // namespace

class CollaboratorService::Impl
{
public:
    explicit Impl (Configuration configuration) : config (std::move (configuration)) {}

    ~Impl() { stop(); }

    Impl (const Impl&) = delete;
    Impl& operator= (const Impl&) = delete;

    void start();
    void stop();

    void setMethodHandler (std::string method, MethodHandler handler);

    RpcOutcome configure (const std::string& model, const Json& systemPromptParams);
    RpcOutcome shutdownSidecar();

    [[nodiscard]] bool isSidecarRunning() const;
    [[nodiscard]] std::optional<int> sidecarProcessId() const;
    [[nodiscard]] std::thread::id serviceThreadId() const { return worker.get_id(); }

private:
    /** What the service thread sees on one turn of its loop. */
    struct Activity
    {
        bool listenReadable = false;
        bool connectionReadable = false;
    };

    void run();
    Activity waitForActivity();
    void wake();
    void drainWake();

    void acceptConnection();
    void readFromConnection();
    void flushOutgoing();
    void handleMessage (const std::string& line);
    void handleRequest (const Json& message);
    void completePending (const Json& message);
    void enqueue (const Json& message);
    void sendError (const Json& id, int code, const std::string& message);

    void processCommands();
    void spawnSidecar();
    void reapSidecar();
    void dropConnection();
    void dropSidecar();

    std::optional<RpcOutcome> ensureSidecar();
    RpcOutcome sendRequestAndWait (const std::string& method, const Json& params);
    void failPendingLocked (int code, const std::string& message);

    struct Pending
    {
        bool done = false;
        RpcOutcome outcome;
    };

    Configuration config;

    // The service thread's own, touched nowhere else once it is running.
    LocalSocketServer listener;
    UniqueFd connection;
    LineReader reader;
    SidecarProcess sidecar;

    UniqueFd wakeDescriptor;
    std::thread worker;

    mutable std::mutex mutex;
    std::condition_variable changed;

    bool started = false;
    bool stopRequested = false;
    bool spawnRequested = false;
    bool terminateRequested = false;
    bool connected = false;
    bool sidecarLive = false;
    bool sidecarStartFailed = false;
    std::optional<int> sidecarPid;
    std::string outgoing;
    std::map<std::string, MethodHandler> handlers;
    std::map<int, Pending> pending;
    int nextRequestId = 1;
};

void CollaboratorService::Impl::start()
{
    {
        const std::lock_guard lock (mutex);

        if (started)
            return;
    }

    wakeDescriptor = UniqueFd { ::eventfd (0, EFD_CLOEXEC | EFD_NONBLOCK) };

    if (! wakeDescriptor.valid())
        throw std::runtime_error ("Collaborator service could not make its wake descriptor: "
                                  + std::generic_category().message (errno));

    listener.listenAt (config.socketPath);

    {
        const std::lock_guard lock (mutex);
        started = true;
        stopRequested = false;
    }

    worker = std::thread ([this] { run(); });
}

void CollaboratorService::Impl::stop()
{
    {
        const std::lock_guard lock (mutex);

        if (! started)
            return;

        stopRequested = true;
    }

    wake();

    if (worker.joinable())
        worker.join();

    const std::lock_guard lock (mutex);
    started = false;
}

void CollaboratorService::Impl::run()
{
    while (true)
    {
        const auto activity = waitForActivity();

        if (activity.listenReadable)
            acceptConnection();

        if (activity.connectionReadable)
            readFromConnection();

        flushOutgoing();
        processCommands();
        reapSidecar();

        const std::lock_guard lock (mutex);

        if (stopRequested)
            break;
    }

    dropSidecar();
    listener.close();
}

CollaboratorService::Impl::Activity CollaboratorService::Impl::waitForActivity()
{
    std::array<pollfd, 3> descriptors { pollfd { wakeDescriptor.get(), POLLIN, 0 },
                                        pollfd { listener.descriptor(), POLLIN, 0 },
                                        pollfd { connection.get(), POLLIN, 0 } };

    {
        const std::lock_guard lock (mutex);

        if (! outgoing.empty())
            descriptors.at (2).events |= POLLOUT;
    }

    ::poll (descriptors.data(), descriptors.size(), pollIntervalMilliseconds);

    if ((descriptors.at (0).revents & POLLIN) != 0)
        drainWake();

    constexpr auto readable = static_cast<short> (POLLIN | POLLHUP | POLLERR);

    return { (descriptors.at (1).revents & POLLIN) != 0,
             (descriptors.at (2).revents & readable) != 0 };
}

void CollaboratorService::Impl::wake()
{
    if (! wakeDescriptor.valid())
        return;

    const std::uint64_t one = 1;
    [[maybe_unused]] const auto written = ::write (wakeDescriptor.get(), &one, sizeof (one));
}

void CollaboratorService::Impl::drainWake()
{
    std::uint64_t value = 0;

    while (::read (wakeDescriptor.get(), &value, sizeof (value)) > 0)
    {
        // Every wake means the same thing: look again.
    }
}

void CollaboratorService::Impl::acceptConnection()
{
    auto incoming = listener.accept();

    if (! incoming.valid())
        return;

    {
        const std::lock_guard lock (mutex);

        // One sidecar at a time. The refused connection closes as this returns,
        // and the connected one is not touched.
        if (connected)
            return;

        connection = std::move (incoming);
        connected = true;
    }

    reader.reset();
    changed.notify_all();
}

void CollaboratorService::Impl::readFromConnection()
{
    std::array<char, readChunkBytes> buffer {};

    while (true)
    {
        const auto count = ::read (connection.get(), buffer.data(), buffer.size());

        if (count > 0)
        {
            for (const auto& line :
                 reader.consume ({ buffer.data(), static_cast<std::size_t> (count) }))
                handleMessage (line);

            continue;
        }

        if (count < 0 && errno == EINTR)
            continue;

        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return;

        // Nothing more will come: the sidecar closed, or died holding it open.
        dropSidecar();

        return;
    }
}

void CollaboratorService::Impl::flushOutgoing()
{
    while (true)
    {
        std::string chunk;

        {
            const std::lock_guard lock (mutex);

            if (outgoing.empty() || ! connection.valid())
                return;

            chunk = outgoing;
        }

        const auto count = ::send (connection.get(), chunk.data(), chunk.size(), MSG_NOSIGNAL);

        if (count > 0)
        {
            const std::lock_guard lock (mutex);
            outgoing.erase (0, static_cast<std::size_t> (count));
            continue;
        }

        if (count < 0 && errno == EINTR)
            continue;

        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return;

        dropSidecar();

        return;
    }
}

void CollaboratorService::Impl::handleMessage (const std::string& line)
{
    if (line.find_first_not_of (" \t") == std::string::npos)
        return;

    const auto message = Json::parse (line, nullptr, false);

    if (message.is_discarded())
    {
        sendError (Json(), rpcError::parseError, "The line was not JSON.");
        return;
    }

    if (! message.is_object() || message.value ("jsonrpc", std::string {}) != "2.0")
    {
        sendError (message.is_object() ? message.value ("id", Json()) : Json(),
                   rpcError::invalidRequest,
                   "The message was not a JSON-RPC 2.0 object.");
        return;
    }

    if (message.contains ("method"))
    {
        handleRequest (message);
        return;
    }

    if (message.contains ("result") || message.contains ("error"))
    {
        completePending (message);
        return;
    }

    sendError (message.value ("id", Json()),
               rpcError::invalidRequest,
               "The message was neither a request nor a response.");
}

void CollaboratorService::Impl::handleRequest (const Json& message)
{
    const auto method = message.value ("method", std::string {});
    const auto id = message.value ("id", Json());

    MethodHandler handler;

    {
        const std::lock_guard lock (mutex);
        const auto found = handlers.find (method);

        if (found != handlers.end())
            handler = found->second;
    }

    if (! handler)
    {
        if (! id.is_null())
            sendError (id, rpcError::methodNotFound, "Unknown method: " + method);

        return;
    }

    RpcOutcome outcome = RpcOutcome::failure (rpcError::internalError, "The handler threw.");

    try
    {
        outcome = handler (message.value ("params", Json::object()));
    }
    catch (const std::exception& failure)
    {
        outcome = RpcOutcome::failure (rpcError::internalError, failure.what());
    }

    if (id.is_null())
        return;

    if (! outcome.succeeded)
    {
        sendError (id, outcome.error.code, outcome.error.message);
        return;
    }

    Json response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = outcome.result;
    enqueue (response);
}

void CollaboratorService::Impl::completePending (const Json& message)
{
    if (! message.contains ("id") || ! message["id"].is_number_integer())
        return;

    {
        const std::lock_guard lock (mutex);
        const auto found = pending.find (message["id"].get<int>());

        if (found == pending.end())
            return;

        found->second.outcome = outcomeFrom (message);
        found->second.done = true;
    }

    changed.notify_all();
}

void CollaboratorService::Impl::enqueue (const Json& message)
{
    {
        const std::lock_guard lock (mutex);
        outgoing += message.dump();
        outgoing += '\n';
    }

    wake();
}

void CollaboratorService::Impl::sendError (const Json& id, int code, const std::string& message)
{
    Json response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["error"] = Json { { "code", code }, { "message", message } };
    enqueue (response);
}

void CollaboratorService::Impl::processCommands()
{
    bool wantsSpawn = false;
    bool wantsTermination = false;

    {
        const std::lock_guard lock (mutex);
        wantsSpawn = std::exchange (spawnRequested, false);
        wantsTermination = std::exchange (terminateRequested, false);
    }

    if (wantsTermination)
        dropSidecar();

    if (wantsSpawn)
        spawnSidecar();
}

void CollaboratorService::Impl::spawnSidecar()
{
    if (sidecar.running())
        return;

    std::vector<std::string> arguments { config.socketPath.string() };
    arguments.insert (
        arguments.end(), config.sidecar.arguments.begin(), config.sidecar.arguments.end());

    const auto spawned = sidecar.spawn (config.sidecar.executable, arguments);

    {
        const std::lock_guard lock (mutex);
        sidecarStartFailed = ! spawned;
        sidecarLive = spawned;
        sidecarPid = spawned ? sidecar.processId() : std::nullopt;
    }

    changed.notify_all();
}

void CollaboratorService::Impl::reapSidecar()
{
    {
        const std::lock_guard lock (mutex);

        if (! sidecarLive)
            return;
    }

    if (sidecar.running())
        return;

    {
        const std::lock_guard lock (mutex);
        sidecarLive = false;
        sidecarPid.reset();

        // A sidecar that exited before it ever called home cannot be waited for.
        if (! connected)
            sidecarStartFailed = true;
    }

    changed.notify_all();
}

void CollaboratorService::Impl::dropConnection()
{
    connection.close();
    reader.reset();

    {
        const std::lock_guard lock (mutex);
        connected = false;
        outgoing.clear();
        failPendingLocked (rpcError::sidecarUnavailable, "The sidecar stopped answering.");
    }

    changed.notify_all();
}

void CollaboratorService::Impl::dropSidecar()
{
    dropConnection();
    sidecar.terminate (config.sidecarStopTimeout);

    {
        const std::lock_guard lock (mutex);
        sidecarLive = false;
        sidecarPid.reset();
    }

    changed.notify_all();
}

void CollaboratorService::Impl::failPendingLocked (int code, const std::string& message)
{
    for (auto& entry : pending)
    {
        if (entry.second.done)
            continue;

        entry.second.outcome = RpcOutcome::failure (code, message);
        entry.second.done = true;
    }
}

std::optional<RpcOutcome> CollaboratorService::Impl::ensureSidecar()
{
    std::unique_lock lock (mutex);

    if (! started)
        return RpcOutcome::failure (rpcError::sidecarUnavailable,
                                    "The Collaborator service is not started.");

    if (connected)
        return std::nullopt;

    sidecarStartFailed = false;
    spawnRequested = true;
    lock.unlock();

    wake();

    lock.lock();
    const auto settled = changed.wait_for (
        lock, config.sidecarStartTimeout, [this] { return connected || sidecarStartFailed; });

    if (! settled)
        return RpcOutcome::failure (rpcError::sidecarUnavailable,
                                    "The sidecar did not connect in time.");

    if (! connected)
        return RpcOutcome::failure (rpcError::sidecarUnavailable, "The sidecar could not start.");

    return std::nullopt;
}

RpcOutcome CollaboratorService::Impl::sendRequestAndWait (const std::string& method,
                                                          const Json& params)
{
    int id = 0;

    {
        const std::lock_guard lock (mutex);

        if (! connected)
            return RpcOutcome::failure (rpcError::sidecarUnavailable, "No sidecar is connected.");

        id = nextRequestId++;
        pending[id] = {};

        Json request;
        request["jsonrpc"] = "2.0";
        request["id"] = id;
        request["method"] = method;
        request["params"] = params;

        outgoing += request.dump();
        outgoing += '\n';
    }

    wake();

    std::unique_lock lock (mutex);
    const auto answered =
        changed.wait_for (lock, config.requestTimeout, [this, id] { return pending.at (id).done; });

    if (! answered)
    {
        pending.erase (id);

        return RpcOutcome::failure (rpcError::sidecarUnavailable,
                                    "The sidecar did not answer in time.");
    }

    auto outcome = std::move (pending.at (id).outcome);
    pending.erase (id);

    return outcome;
}

void CollaboratorService::Impl::setMethodHandler (std::string method, MethodHandler handler)
{
    const std::lock_guard lock (mutex);
    handlers[std::move (method)] = std::move (handler);
}

RpcOutcome CollaboratorService::Impl::configure (const std::string& model,
                                                 const Json& systemPromptParams)
{
    if (const auto failure = ensureSidecar())
        return *failure;

    Json params;
    params["model"] = model;
    params["systemPromptParams"] = systemPromptParams;

    return sendRequestAndWait ("configure", params);
}

RpcOutcome CollaboratorService::Impl::shutdownSidecar()
{
    bool wasConnected = false;

    {
        const std::lock_guard lock (mutex);

        // Nothing to shut down, and nothing to start in order to shut it down.
        if (! connected && ! sidecarLive)
            return RpcOutcome::success (Json::object());

        wasConnected = connected;
    }

    auto outcome = RpcOutcome::success (Json::object());

    if (wasConnected)
        outcome = sendRequestAndWait ("shutdown", Json::object());

    std::unique_lock lock (mutex);

    if (changed.wait_for (lock, config.sidecarStopTimeout, [this] { return ! sidecarLive; }))
        return outcome;

    terminateRequested = true;
    lock.unlock();

    wake();

    lock.lock();
    changed.wait_for (lock, config.sidecarStopTimeout, [this] { return ! sidecarLive; });

    return outcome;
}

bool CollaboratorService::Impl::isSidecarRunning() const
{
    const std::lock_guard lock (mutex);

    return sidecarLive;
}

std::optional<int> CollaboratorService::Impl::sidecarProcessId() const
{
    const std::lock_guard lock (mutex);

    return sidecarPid;
}

CollaboratorService::CollaboratorService (Configuration configuration)
    : impl (std::make_unique<Impl> (std::move (configuration)))
{
}

CollaboratorService::~CollaboratorService() = default;

void CollaboratorService::start() { impl->start(); }

void CollaboratorService::stop() { impl->stop(); }

void CollaboratorService::setMethodHandler (std::string method, MethodHandler handler)
{
    impl->setMethodHandler (std::move (method), std::move (handler));
}

RpcOutcome CollaboratorService::configure (const std::string& model, const Json& systemPromptParams)
{
    return impl->configure (model, systemPromptParams);
}

RpcOutcome CollaboratorService::shutdownSidecar() { return impl->shutdownSidecar(); }

bool CollaboratorService::isSidecarRunning() const { return impl->isSidecarRunning(); }

std::optional<int> CollaboratorService::sidecarProcessId() const
{
    return impl->sidecarProcessId();
}

std::thread::id CollaboratorService::serviceThreadId() const { return impl->serviceThreadId(); }
} // namespace duet::collab
