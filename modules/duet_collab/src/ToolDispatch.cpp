#include <duet/collab/ToolDispatch.h>

#include <utility>

namespace duet::collab
{
namespace
{
    /** A string member of an object, and empty for anything else.

        Anything else includes a member that is a number or an object, which is
        the sidecar sending something the contract does not allow: JSON's own
        accessor throws on that, and there is nothing here worth throwing over.
    */
    std::string stringMember (const Json& object, const char* name)
    {
        const auto found = object.find (name);

        if (found == object.end() || ! found->is_string())
            return {};

        return found->get<std::string>();
    }
} // namespace

void ToolRegistry::add (std::string name, Tool tool) { tools[std::move (name)] = std::move (tool); }

void ToolRegistry::clear() { tools.clear(); }

RpcOutcome ToolRegistry::call (const Json& params) const
{
    ToolCall toolCall;

    if (params.is_object())
    {
        toolCall.runId = stringMember (params, "runId");
        toolCall.callId = stringMember (params, "callId");
        toolCall.tool = stringMember (params, "tool");

        if (const auto arguments = params.find ("args");
            arguments != params.end() && arguments->is_object())
            toolCall.arguments = *arguments;
    }

    if (toolCall.tool.empty())
        return RpcOutcome::failure (rpcError::invalidParams, "a tool call needs a tool name");

    const auto found = tools.find (toolCall.tool);

    if (found == tools.end())
        return RpcOutcome::failure (rpcError::unknownTool,
                                    "there is no tool called " + toolCall.tool);

    return found->second (toolCall);
}
} // namespace duet::collab
