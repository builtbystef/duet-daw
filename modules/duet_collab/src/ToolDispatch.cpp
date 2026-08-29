#include <duet/collab/ToolDispatch.h>

#include <memory>
#include <mutex>
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

std::shared_ptr<const ToolRegistry::Vocabulary> ToolRegistry::held() const
{
    const std::lock_guard lock (swapMutex);

    return vocabulary;
}

void ToolRegistry::replace (Vocabulary next)
{
    auto replacement = std::make_shared<const Vocabulary> (std::move (next));
    const std::lock_guard lock (swapMutex);
    vocabulary = std::move (replacement);
}

void ToolRegistry::add (std::string name, Tool tool)
{
    Vocabulary next = *held();
    next.tools[std::move (name)] = std::move (tool);
    replace (std::move (next));
}

void ToolRegistry::hold (std::shared_ptr<void> whatTheyRead)
{
    Vocabulary next = *held();
    next.read = std::move (whatTheyRead);
    replace (std::move (next));
}

void ToolRegistry::clear() { replace (Vocabulary {}); }

RpcOutcome ToolRegistry::call (const Json& params) const
{
    // The hold is taken before the name is even read, and let go only when the
    // tool has answered: what the tools read cannot be taken away underneath
    // one, whatever the message thread does meanwhile.
    const auto vocabularyHeld = held();

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

    const auto found = vocabularyHeld->tools.find (toolCall.tool);

    if (found == vocabularyHeld->tools.end())
        return RpcOutcome::failure (rpcError::unknownTool,
                                    "there is no tool called " + toolCall.tool);

    return found->second (toolCall);
}
} // namespace duet::collab
