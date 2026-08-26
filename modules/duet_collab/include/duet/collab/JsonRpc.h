#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <utility>

namespace duet::collab
{
/** The payload vocabulary of the seam.

    Everything that crosses the socket is JSON, in both directions, so the type
    is part of this module's public interface rather than hidden behind it: a
    tool result and a Task Run event are JSON objects and there is nothing to be
    gained by copying them into a second shape on the way through.
*/
using Json = nlohmann::json;

/** The JSON-RPC 2.0 error codes this seam answers with.

    The first five are the specification's own. The last two are Duet's, and sit
    inside the -32099..-32000 range the specification reserves for an
    implementation's own server errors.
*/
namespace rpcError
{
    inline constexpr int parseError = -32700;
    inline constexpr int invalidRequest = -32600;
    inline constexpr int methodNotFound = -32601;
    inline constexpr int invalidParams = -32602;
    inline constexpr int internalError = -32603;
    inline constexpr int sidecarUnavailable = -32000;
    inline constexpr int runAlreadyActive = -32001;
} // namespace rpcError

/** The error member of a JSON-RPC response. */
struct RpcError
{
    int code = 0;
    std::string message;
};

/** What one request across the seam came back as: a result, or an error.

    A request that never reaches a sidecar — because none is there, or because
    the one that is there stopped answering — fails with `sidecarUnavailable`
    rather than throwing, so the caller handles a dead sidecar and a refusing
    one through the same branch.
*/
struct RpcOutcome
{
    bool succeeded = false;
    Json result;
    RpcError error;

    [[nodiscard]] static RpcOutcome success (Json value) { return { true, std::move (value), {} }; }

    [[nodiscard]] static RpcOutcome failure (int code, std::string message)
    {
        return { false, Json::object(), { code, std::move (message) } };
    }
};
} // namespace duet::collab
