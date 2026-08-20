#pragma once

#include <duet/collab/JsonRpc.h>

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace duet::collab
{
/** How the service starts the sidecar.

    The socket path goes in front of `arguments`, so a sidecar always reads the
    path to call home on as its first argument (ADR 0003).
*/
struct SidecarLaunch
{
    std::filesystem::path executable;
    std::vector<std::string> arguments;
};

/** The DAW half of the AI seam: a JSON-RPC 2.0 peer on a local socket, and the
    owner of the sidecar process that connects to it (ADR 0003).

    The DAW is the server. `start()` puts a Unix domain socket at the configured
    path and listens; the sidecar is spawned only when something first asks it a
    question, is terminated when this object goes away, and is spawned again by
    the next question if it was found dead in between. One sidecar is connected
    at a time: while one is, a second connection is accepted and immediately
    closed, and the connected one never notices.

    Both directions travel over that one connection. `configure` and
    `shutdownSidecar` are requests this side sends; requests the sidecar sends —
    `tool.call` and, later, the rest of the Tool Vocabulary — are answered by the
    handlers registered through `setMethodHandler`.

    Threading: every byte read or written, every line framed, and every fork of
    the sidecar happens on this service's own thread, which `serviceThreadId()`
    names and which handlers are called on. Nothing here runs on the message
    thread or the audio thread, and this module links no engine and no JUCE, so
    it shares no lock with the audio callback by construction. The request calls
    are the exception a caller can see: they block the calling thread until the
    answer arrives or the timeout expires.
*/
class CollaboratorService
{
public:
    struct Configuration
    {
        std::filesystem::path socketPath;
        SidecarLaunch sidecar;

        /** How long a request waits for its answer before failing. */
        std::chrono::milliseconds requestTimeout { 30000 };

        /** How long a spawned sidecar has to connect before the request that
            spawned it gives up.
        */
        std::chrono::milliseconds sidecarStartTimeout { 10000 };

        /** How long a sidecar asked to stop has to go before it is killed. */
        std::chrono::milliseconds sidecarStopTimeout { 5000 };
    };

    /** Answers one method the sidecar calls. Called on the service thread. */
    using MethodHandler = std::function<RpcOutcome (const Json& params)>;

    explicit CollaboratorService (Configuration configuration);
    ~CollaboratorService();

    CollaboratorService (const CollaboratorService&) = delete;
    CollaboratorService& operator= (const CollaboratorService&) = delete;

    /** Puts the socket in place and starts the service thread. Throws
        `std::runtime_error` when the socket cannot be made.
    */
    void start();

    /** Terminates the sidecar, closes the socket and removes the socket file.
        Called by the destructor; calling it twice is harmless.
    */
    void stop();

    /** Registers the answer to one method the sidecar may call. Register before
        `start()`, or accept that a call arriving first is answered with
        method-not-found.
    */
    void setMethodHandler (std::string method, MethodHandler handler);

    /** The sidecar's `configure`. Spawns the sidecar if none is running. */
    RpcOutcome configure (const std::string& model, const Json& systemPromptParams);

    /** The sidecar's `shutdown`, and the wait for it to go. Spawns nothing: with
        no sidecar running this succeeds and does nothing.
    */
    RpcOutcome shutdownSidecar();

    [[nodiscard]] bool isSidecarRunning() const;

    /** The sidecar's process id while one is running, and nothing otherwise. */
    [[nodiscard]] std::optional<int> sidecarProcessId() const;

    /** The thread every socket, framing and process operation happens on. */
    [[nodiscard]] std::thread::id serviceThreadId() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace duet::collab
