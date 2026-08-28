#pragma once

#include <duet/collab/Estimate.h>
#include <duet/collab/JsonRpc.h>
#include <duet/collab/TaskRun.h>

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
    handlers registered through `setMethodHandler`. A Task Run is the third
    shape: `startRun` and `cancelRun` are commands rather than questions, and
    what a run has to say comes back as a `TaskRunListener`'s calls.

    Threading: every byte read or written, every line framed, and every fork of
    the sidecar happens on this service's own thread, which `serviceThreadId()`
    names and which handlers and the listener are called on. Nothing here runs on
    the message thread or the audio thread, and this module links no engine and
    no JUCE, so it shares no lock with the audio callback by construction.

    `configure` and `shutdownSidecar` are the only calls that block their caller,
    and they block until the answer arrives or the timeout expires. Nothing about
    a Task Run ever does: the DAW is never held up by the Collaborator.
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

    /** The one listener every Task Run reports to, read and never owned. Set it
        before `start()`, or accept that a run begun first reports to nobody.

        Clearing it waits for the call in flight, so a listener that is cleared
        before it is destroyed is never called again. Do not call this from
        inside a listener's own callback.
    */
    void setTaskRunListener (TaskRunListener* newListener);

    /** Registers the ledger this service marks its runs from, read and never
        owned. Set it before `start()`, and let it outlive the service.

        The service is where the mark is applied, because the mark must not
        depend on the model's cooperation: a run whose ledger holds an estimate
        has everything it says from then on marked as based on estimates, and
        every run begins with its own ledger emptied, so no run inherits the one
        before it. With no ledger registered nothing is ever marked, which is
        what a service with no estimating tool wired to it should say.
    */
    void setEstimateLedger (EstimateLedger* ledger);

    /** Begins a Task Run, and returns without waiting for anything.

        Nothing here touches the sidecar on the calling thread: the run is handed
        to the service thread, which spawns a sidecar if none is connected and
        sends `run.start` when one is. Completion arrives later, and only ever as
        the listener's terminal event.

        One run at a time: while one is in progress this rejects with
        `runAlreadyActive` and the run in progress is untouched.
    */
    [[nodiscard]] RunStart startRun (const std::string& prompt, const OpeningContext& context);

    /** Ends the named run as canceled, and returns without waiting for anything.

        Cancellation is client-side: the run ends here and the listener is told,
        while `run.cancel` goes to the sidecar to stop the work. Whatever that
        sidecar says about the run afterwards is ignored.

        Returns whether this call is the one that ended it, so a second cancel of
        the same run says false and does nothing.
    */
    bool cancelRun (const std::string& runId);

    /** The run in progress, and nothing when none is. */
    [[nodiscard]] std::optional<std::string> activeRunId() const;

    /** Whether the named run is the one in progress and nobody has asked for it
        to stop.

        What a tool that takes seconds asks before it goes on working. A cancel
        lands here the moment it is asked for, under this object's own lock,
        while the service thread may still be inside the tool that would like to
        know — so this and not `activeRunId` is what tells a long tool call that
        nobody is waiting for it any more.
    */
    [[nodiscard]] bool isRunInProgress (const std::string& runId) const;

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
