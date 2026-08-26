#pragma once

#include <duet/collab/JsonRpc.h>

#include <functional>
#include <map>
#include <string>

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
    marshals onto the message thread itself; this registry knows nothing about
    threads and holds no lock.
*/
class ToolRegistry
{
public:
    /** Answers one tool call. Called on the service thread. */
    using Tool = std::function<RpcOutcome (const ToolCall& call)>;

    /** Adds a tool, replacing one of the same name. */
    void add (std::string name, Tool tool);

    /** Answers a `tool.call` request's params.

        Params without a tool name are `invalidParams`; a name that is not in
        the vocabulary is `unknownTool`. Everything else is the tool's own
        answer, error included.
    */
    [[nodiscard]] RpcOutcome call (const Json& params) const;

private:
    std::map<std::string, Tool> tools;
};
} // namespace duet::collab
