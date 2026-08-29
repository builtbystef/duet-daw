#pragma once

#include <duet/collab/JsonRpc.h>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace duet::collab
{
/** One call of the Tool Vocabulary, as the sidecar asked it.

    `runId` and `callId` are the sidecar's own and nothing here reads them: a
    tool answers about the project, not about the run that asked. They are
    carried so that a handler which does need them — the estimate ledger, when
    it arrives — has them without a change to this shape.
*/
struct ToolCall
{
    std::string runId;
    std::string callId;
    std::string tool;
    Json arguments = Json::object();
};

/** The closed set of tools the Collaborator may call, and the dispatch to them.

    The Tool Vocabulary is closed by construction: a name that is not in this
    registry is answered with `unknownTool` rather than with silence or a guess,
    and the connection carrying it is untouched, so a run survives its model
    asking for a tool that does not exist.

    Every tool is called on the Collaborator service's own thread, because that
    is the thread `tool.call` arrives on. A tool that reads the project model
    marshals onto the message thread itself.

    The vocabulary is swapped from the message thread, when the producer opens
    or creates another project, and that swap may not wait for a call that is
    inside a tool: a measurement renders, and tearing a render down needs the
    message loop running, so a message thread waiting here would be waiting for
    itself. So the whole vocabulary — the tools and the thing they read — is
    replaced as one, and a call already inside a tool goes on holding the one it
    entered until it returns. The lock is over the swap alone and is never held
    while a tool runs.
*/
class ToolRegistry
{
public:
    /** Answers one tool call. Called on the service thread. */
    using Tool = std::function<RpcOutcome (const ToolCall& call)>;

    /** Adds a tool, replacing one of the same name. */
    void add (std::string name, Tool tool);

    /** Holds what the tools read for as long as this registry may answer with
        them, and for as long as a call is inside one.

        A tool is a callable over the object that answers it, and that object
        reads a project. Handing both here is what lets the owner take them away
        without waiting: the last call out of the vocabulary is what puts them
        down, wherever it happens to be.
    */
    void hold (std::shared_ptr<void> whatTheyRead);

    /** Forgets every tool, and lets go of what they read.

        A tool is a callable over the object that answers it, so a registry
        outliving those objects answers a call by reaching into freed memory.
        Whoever owns both clears the registry when it takes the objects away.
    */
    void clear();

    /** Answers a `tool.call` request's params.

        Params without a tool name are `invalidParams`; a name that is not in
        the vocabulary is `unknownTool`. Everything else is the tool's own
        answer, error included.
    */
    [[nodiscard]] RpcOutcome call (const Json& params) const;

private:
    /** One project's whole vocabulary: the tools, and the thing they were built
        over. Replaced as one, never edited in place, so that a call holding it
        holds a set that cannot change under it.
    */
    struct Vocabulary
    {
        std::map<std::string, Tool> tools;
        std::shared_ptr<void> read;
    };

    /** A hold on the vocabulary of the moment, taken under the lock and used
        outside it.
    */
    [[nodiscard]] std::shared_ptr<const Vocabulary> held() const;

    void replace (Vocabulary next);

    mutable std::mutex swapMutex;
    std::shared_ptr<const Vocabulary> vocabulary = std::make_shared<const Vocabulary>();
};
} // namespace duet::collab
